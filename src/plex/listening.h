// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "audio.h"
#include "home.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctime>
#include <cerrno>
namespace plex {
// Only stable identities and local listening positions. Never tokens, URLs or PINs.
class ListeningStore {
    Json records=Json::object();
    std::string file;
    bool writable=false;
public:
    bool load(const std::string &path) {
        file=path;records=Json::object();writable=false;int fd=open(file.c_str(),O_RDONLY|O_NOFOLLOW);
        if(fd<0){writable=errno==ENOENT;return false;}
        struct stat st{};if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || (st.st_mode&077)!=0 || st.st_size<0 || st.st_size>524288){close(fd);return false;}
        std::string bytes(static_cast<size_t>(st.st_size),'\0');size_t at=0;
        while(at<bytes.size()){ssize_t n=read(fd,&bytes[at],bytes.size()-at);if(n<=0){close(fd);return false;}at+=n;}
        close(fd);auto parsed=Json::parse(bytes,nullptr,false);
        if(parsed.is_discarded() || !parsed.is_object() || parsed.size()>512)return false;
        records=std::move(parsed);writable=true;return true;
    }
    bool save() const {
        if(file.empty() || !writable)return false;
        auto bytes=records.dump();if(bytes.size()>524288)return false;
        auto tmp=file+".tmp";int fd=open(tmp.c_str(),O_WRONLY|O_CREAT|O_NOFOLLOW,0600);if(fd<0)return false;
        struct stat st{};bool ok=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==geteuid() && st.st_nlink==1;
        if(!ok || fchmod(fd,0600)!=0 || ftruncate(fd,0)!=0){close(fd);return false;}
        size_t at=0;while(at<bytes.size()){ssize_t n=write(fd,bytes.data()+at,bytes.size()-at);if(n<=0){ok=false;break;}at+=n;}
        if(fsync(fd)!=0)ok=false;
        if(close(fd)!=0)ok=false;
        if(ok && rename(tmp.c_str(),file.c_str())==0)return true;
        unlink(tmp.c_str());return false;
    }
    static std::string key(const std::string &scope,const std::string &book) {return encode(scope)+"/"+encode(book);}
    Json get(const std::string &scope,const std::string &book) const {
        auto r=records.find(key(scope,book));return r!=records.end() && r->is_object()?*r:Json::object();
    }
    bool put(const std::string &scope,const std::string &book,Json value) {
        if(scope.empty() || scope.size()>512 || book.empty() || book.size()>128 || !value.is_object())return false;
        auto k=key(scope,book);if(!records.contains(k) && records.size()>=512)return false;
        records[k]=std::move(value);return save();
    }
    std::vector<Item> books(const std::string &scope,int state=0) const {
        std::vector<Item> out;auto prefix=encode(scope)+"/";
        for(auto it=records.begin();it!=records.end();++it) {
            if(it.key().rfind(prefix,0)!=0 || !it->is_object())continue;
            bool done=flag(*it,"finished",false);
            if((state==0 && done) || (state==1 && !done))continue;
            Item i;i.id=text(*it,"book");i.type=text(*it,"kind","album");i.title=text(*it,"title","Audiobook");i.thumb=text(*it,"thumb");
            i.offset=number(*it,"total_position");i.duration=number(*it,"total_duration");i.watched=done;
            if(!i.id.empty())out.push_back(i);
        }
        if(out.size()>200)out.resize(200);
        return out;
    }
};
struct BookResume {size_t index=0;int64_t position=0;bool local=false;};
inline BookResume book_resume(const std::vector<Item> &tracks,const Json &local) {
    // A newer observed server track timestamp takes precedence over stale local data.
    size_t server_index=0;int64_t latest=0;
    for(size_t n=0;n<tracks.size();++n)if(tracks[n].last_played>latest){latest=tracks[n].last_played;server_index=n;}
    auto updated=number(local,"updated");auto id=text(local,"track");
    for(size_t n=0;n<tracks.size();++n)if(tracks[n].id==id && updated>=latest && !flag(local,"finished",false))
        return {n,std::clamp<int64_t>(number(local,"position"),0,tracks[n].duration>0?tracks[n].duration:INT64_MAX),true};
    return tracks.empty()?BookResume{}:BookResume{server_index,tracks[server_index].offset,false};
}
struct SleepTimer {
    double deadline=0;bool end_track=false;
    void set(int minutes,double now) {deadline=minutes>0?now+minutes*60:0;end_track=minutes==-1;}
    bool due(double now,bool track_ended=false) const {return (deadline>0 && now>=deadline) || (end_track && track_ended);}
    void clear(){deadline=0;end_track=false;}
};
inline float book_speed(int n) {const float speeds[]={.75f,1.f,1.10f,1.25f,1.5f,1.75f,2.f};return speeds[std::clamp(n,0,6)];}
}
