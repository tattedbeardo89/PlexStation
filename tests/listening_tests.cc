// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/listening.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace plex;
int main() {
 char dir[]="/tmp/plexstation-listening-XXXXXX";assert(mkdtemp(dir));auto path=std::string(dir)+"/listening.json";
 ListeningStore store;assert(!store.load(path));
 Json value={{"book","album1"},{"title","Book"},{"kind","album"},{"track","t2"},{"position",45000},{"updated",100},{"finished",false}};
 assert(store.put("profile-A/server-A","album1",value));
 struct stat st{};assert(stat(path.c_str(),&st)==0 && (st.st_mode&0777)==0600);
 ListeningStore restored;assert(restored.load(path));assert(number(restored.get("profile-A/server-A","album1"),"position")==45000);
 assert(restored.get("profile-B/server-A","album1").empty());assert(restored.get("profile-A/server-B","album1").empty());
 assert(restored.books("profile-A/server-A").size()==1 && restored.books("profile-B/server-A").empty());
 Item a,b;a.id="t1";a.duration=100000;b.id="t2";b.duration=120000;
 auto resume=book_resume({a,b},value);assert(resume.index==1 && resume.position==45000 && resume.local);
 a.last_played=200;a.offset=60000;resume=book_resume({a,b},value);assert(!resume.local && resume.index==0 && resume.position==60000);
 value["position"]=INT64_MAX;resume=book_resume({b},value);assert(resume.position==120000);
 value["finished"]=true;assert(restored.put("profile-A/server-A","album1",value));assert(restored.books("profile-A/server-A").empty() && restored.books("profile-A/server-A",1).size()==1);
 SleepTimer timer;timer.set(15,100);assert(!timer.due(999) && timer.due(1000));timer.clear();assert(!timer.due(99999));timer.set(-1,0);assert(!timer.due(99999) && timer.due(0,true));
 assert(book_speed(0)==.75f && book_speed(6)==2.f && book_speed(99)==2.f);
 assert(unlink(path.c_str())==0);assert(symlink("/etc/passwd",path.c_str())==0);assert(!restored.load(path));assert(unlink(path.c_str())==0);rmdir(dir);
 puts("Listening persistence/permissions, wrong profile/server, newer server resume, bounds, completion, chapter timer and speeds passed");
}
