#include "subtitle_latch.h"
#include <vector>
#include <cassert>
#include <cstdio>
struct Track{int id;};
int main(){
 SubtitleLatch latch;int current=12,calls=0;
 auto select=[&](int id){current=id;++calls;return true;};
 assert(!latch.enforce(std::vector<Track>{{12}},current,select));assert(calls==0);
 std::vector<Track> tracks{{12},{35},{-42}};
 assert(latch.enforce(tracks,current,select) && current==-42 && calls==1);
 assert(latch.enforce(tracks,current,select) && calls==1);
 for(int event=0;event<4;++event){current=35;assert(latch.enforce(tracks,current,select) && current==-42);}
 latch.off=false;current=12;assert(latch.enforce(tracks,current,select) && current==12);
 latch.off=true;assert(latch.enforce(tracks,current,select) && current==-42);
 latch.choose(false);latch.begin(true,true);assert(!latch.off);
 latch.begin(false,true);assert(latch.off);
 latch.choose(true);latch.begin(true,false);assert(latch.off);
 latch.begin(false,false);assert(!latch.off);
 assert(subtitle_language_matches("eng","en"));
 assert(subtitle_language_matches("pt-BR","pt"));
 assert(subtitle_language_matches("FRE","fr"));
 assert(!subtitle_language_matches("es","en"));
 latch.choose(true);current=35;
 assert(!latch.enforce(tracks,current,[](int){return false;}));
 assert(latch.enforce(tracks,current,select) && current==-42);
 puts("Subtitle Off survives discovery, seek/resume/reactivation; disabled ID is discovered, never fixed");
}
