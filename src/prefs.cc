/*
 * VLC-PS5's remembered choices: prefs.txt, read once, written whole (through a
 * temporary file) on every change.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "prefs.h"

#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <atomic>

#include "platform.h"

namespace {

std::map<std::string, std::string> values;
bool loaded, existed;

std::string path()
{
    return std::string(plat_data_dir()) + "/prefs.txt";
}

void load()
{
    if (loaded)
        return;
    loaded = true;
    int fd=open(path().c_str(),O_RDONLY|O_NOFOLLOW);
    if(fd<0)return;
    struct stat st{};
    if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() || st.st_size>2*1024*1024){close(fd);return;}
    FILE *f = fdopen(fd, "r");
    if (!f){close(fd);return;}
    existed = true;
    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r'))
            l.pop_back();
        size_t eq = l.find('=');
        if (eq != std::string::npos && eq > 0)
            values[l.substr(0, eq)] = l.substr(eq + 1);
    }
    fclose(f);
}

void save()
{
    static std::atomic<unsigned> serial{0};
    std::string tmp = path()+".tmp-"+std::to_string(getpid())+"-"+std::to_string(serial++);
    int fd=open(tmp.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
    if(fd<0)return;
    FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(tmp.c_str());return;}
    bool ok=true;
    for(const auto &kv:values)if(fprintf(f,"%s=%s\n",kv.first.c_str(),kv.second.c_str())<0)ok=false;
    if(fflush(f)!=0 || fsync(fd)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(!ok || rename(tmp.c_str(),path().c_str())!=0)unlink(tmp.c_str());
}

} // namespace

bool prefs_existed()
{
    load();
    return existed;
}

int pref_int(const char *key, int fallback)
{
    load();
    auto it = values.find(key);
    return it == values.end() || it->second.empty() ? fallback : atoi(it->second.c_str());
}

std::string pref_str(const char *key, const char *fallback)
{
    load();
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

void pref_set(const char *key, int value)
{
    pref_set(key, std::to_string(value));
}

void pref_set(const char *key, const std::string &value)
{
    if(!key || !*key || std::string(key).find_first_of("=\r\n")!=std::string::npos || value.size()>4096 || value.find_first_of("\r\n")!=std::string::npos)return;
    load();
    if (values[key] == value)
        return;
    values[key] = value;
    save();
}
