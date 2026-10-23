// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <gnutls/crypto.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
namespace plex {
inline std::string cache_digest(const std::string &value) {
    static std::once_flag once;static int initialized=-1;
    std::call_once(once,[]{initialized=gnutls_global_init();});
    unsigned char digest[32];
    if(initialized<0 || gnutls_hash_fast(GNUTLS_DIG_SHA256,value.data(),value.size(),digest)<0)return {};
    static const char *hex="0123456789abcdef";std::string out;out.reserve(64);
    for(auto byte:digest){out+=hex[byte>>4];out+=hex[byte&15];}return out;
}
// Caller validates content before put. Only private, owned files in a dedicated
// private directory are accepted. Keys/scopes are opaque hashes, never URLs.
class DiskCache {
    std::string root,prefix;
    size_t budget=256u*1024*1024;
    struct File{std::string name;size_t bytes;time_t used;};
    bool directory(bool create=false) const {
        if(root.empty() || prefix.empty())return false;
        if(create && mkdir(root.c_str(),0700)!=0 && errno!=EEXIST)return false;
        struct stat st{};
        return lstat(root.c_str(),&st)==0 && S_ISDIR(st.st_mode) && st.st_uid==geteuid() && (st.st_mode&077)==0;
    }
    std::string path(const std::string &key) const {auto digest=cache_digest(key);return digest.empty()?std::string():root+"/"+prefix+digest+".cache";}
    std::vector<File> files() const {
        std::vector<File> out;if(!directory())return out;
        auto *dir=opendir(root.c_str());if(!dir)return out;
        while(auto *entry=readdir(dir)) {
            std::string name=entry->d_name;
            if(name.size()!=135 || name[64]!='-' || name.substr(129)!=".cache")continue;
            auto hex=name.substr(0,64)+name.substr(65,64);
            if(hex.find_first_not_of("0123456789abcdef")!=std::string::npos)continue;
            struct stat st{};auto full=root+"/"+name;
            if(lstat(full.c_str(),&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || (st.st_mode&077)!=0 || st.st_size<0)continue;
            out.push_back({name,static_cast<size_t>(st.st_size),st.st_mtime});
        }
        closedir(dir);return out;
    }
public:
    DiskCache()=default;
    DiskCache(std::string directory,const std::string &scope,size_t maximum=256u*1024*1024):root(std::move(directory)),budget(maximum) {
        if(!scope.empty()){auto digest=cache_digest(scope);if(!digest.empty())prefix=digest+"-";}
    }
    std::string get(const std::string &key,size_t maximum=8u*1024*1024) const {
        if(!directory())return {};
        auto name=path(key);if(name.empty())return {};
        int fd=open(name.c_str(),O_RDONLY|O_NOFOLLOW);if(fd<0)return {};
        struct stat st{};
        if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || (st.st_mode&077)!=0 || st.st_size<=0 || static_cast<uint64_t>(st.st_size)>maximum){close(fd);return {};}
        std::string bytes(static_cast<size_t>(st.st_size),'\0');size_t at=0;
        while(at<bytes.size()){auto n=read(fd,&bytes[at],bytes.size()-at);if(n<0 && errno==EINTR)continue;if(n<=0){bytes.clear();break;}at+=static_cast<size_t>(n);}
        // Updating mtime is safe with an already-open, ownership-checked file.
        if(!bytes.empty())futimens(fd,nullptr);
        close(fd);return bytes;
    }
    size_t usage() const {size_t bytes=0;for(const auto &file:files())bytes+=file.bytes;return bytes;}
    void trim() const {
        auto list=files();size_t bytes=0;for(const auto &f:list)bytes+=f.bytes;
        std::sort(list.begin(),list.end(),[](const File &a,const File &b){return a.used<b.used;});
        size_t count=list.size();
        for(const auto &f:list){if(bytes<=budget && count<=4096)break;if(unlink((root+"/"+f.name).c_str())==0){bytes-=f.bytes;--count;}}
    }
    bool put(const std::string &key,const std::string &bytes) const {
        if(bytes.empty() || bytes.size()>budget || bytes.size()>8u*1024*1024 || !directory(true))return false;
        auto name=path(key);if(name.empty())return false;
        static std::atomic<unsigned> serial{0};auto temp=name+".tmp-"+std::to_string(getpid())+"-"+std::to_string(serial.fetch_add(1));
        int fd=open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0)return false;
        bool ok=true;size_t at=0;
        while(at<bytes.size()){auto n=write(fd,bytes.data()+at,bytes.size()-at);if(n<0 && errno==EINTR)continue;if(n<=0){ok=false;break;}at+=static_cast<size_t>(n);}
        if(fsync(fd)!=0)ok=false;
        if(close(fd)!=0)ok=false;
        if(ok && rename(temp.c_str(),name.c_str())==0){trim();return true;}
        unlink(temp.c_str());return false;
    }
    void erase(const std::string &key) const {if(directory()){auto name=path(key);if(!name.empty())unlink(name.c_str());}}
    void clear_scope() const {for(const auto &f:files())if(f.name.rfind(prefix,0)==0)unlink((root+"/"+f.name).c_str());}
};
}
