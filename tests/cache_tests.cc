// Production persistent-cache security, isolation and bounded-storage regression.
#include "plex/cache.h"
#include <cassert>
#include <filesystem>
#include <fstream>
int main() {
 char temporary[]="/tmp/plex-cache-tests-XXXXXX";auto parent=mkdtemp(temporary);assert(parent);
 std::string root=std::string(parent)+"/art";
 plex::DiskCache a(root,"account-A/profile-1/server-1",16),b(root,"account-B/profile-1/server-1",16);
 assert(a.put("private-url-token", "image-A"));assert(a.get("private-url-token")=="image-A");
 assert(b.get("private-url-token").empty());
 assert(b.put("other", "image-B"));
 plex::DiskCache reopened(root,"account-A/profile-1/server-1",16);
 assert(reopened.get("private-url-token")=="image-A");assert(reopened.get("private-url-token",2).empty());
 for(const auto &entry:std::filesystem::directory_iterator(root)) {
   auto name=entry.path().filename().string();assert(name.size()==135 && name.find("token")==std::string::npos);
   struct stat st{};assert(lstat(entry.path().c_str(),&st)==0 && (st.st_mode&0777)==0600);
 }
 a.clear_scope();assert(a.get("private-url-token").empty());assert(b.get("other")=="image-B");
 b.clear_scope();
 assert(a.put("large",std::string(16,'a')));assert(a.usage()<=16);
 assert(!a.put("oversize",std::string(17,'x')));
 auto file=root+"/"+plex::cache_digest("account-A/profile-1/server-1")+"-"+plex::cache_digest("large")+".cache";
 assert(chmod(file.c_str(),0644)==0);assert(a.get("large").empty());assert(chmod(file.c_str(),0600)==0);
 assert(unlink(file.c_str())==0);assert(symlink("/etc/passwd",file.c_str())==0);assert(a.get("large").empty());
 auto link=std::string(parent)+"/link";assert(symlink(root.c_str(),link.c_str())==0);
 assert(!plex::DiskCache(link,"scope").put("key","data"));
 std::filesystem::remove_all(parent);
 puts("Disk cache: restart, account isolation, byte budget, private modes, symlinks and bounded reads passed");
}
