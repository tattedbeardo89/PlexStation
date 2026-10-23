#include "subtitle_latch.h"
#include "plex/subtitles.h"
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
 plex::Item movie;movie.id="movie";movie.media=plex::Json::array({{{"Part",plex::Json::array({{{"Stream",plex::Json::array({
 {{"id",101},{"streamType",3},{"languageCode","eng"},{"language","English"},{"title","Commentary"},{"forced",true},{"hearingImpaired",true}},
 {{"id",102},{"streamType",3},{"languageCode","spa"},{"key","/library/streams/102"},{"codec","srt"}},
 {{"id",103},{"streamType",2},{"languageCode","eng"}}
 })}}})}}});
 auto streams=plex::subtitle_streams(movie,0);assert(streams.size()==2);
 assert(streams[0].label.find("Forced")!=std::string::npos && streams[0].label.find("Hearing impaired")!=std::string::npos);
 struct Native {int id;std::string name,language;};
 std::vector<Native> native{{7,"Commentary","en"},{42,"Spanish","es"}};
 assert(plex::match_subtitle(streams[0],native)==7); // Plex 101 is never sent to VLC.
 native.push_back({8,"Commentary","eng"});assert(plex::match_subtitle(streams[0],native)==-1);
 assert(plex::match_subtitle(streams[1],native)==42);
 assert(plex::subtitle_streams(movie,1).empty());
 plex::SubtitleSelection selection;selection.begin("movie",0,0);assert(selection.choice==-1);
 selection.choice=1;selection.begin("movie",0,0);assert(selection.choice==1);
 selection.begin("episode",0,0);assert(selection.choice==-1);
 selection.begin("episode",1,1);assert(selection.choice==-2);
 selection.reset();selection.begin("movie",0,2);assert(selection.choice==-3);
 puts("Subtitle Off survives discovery, seek/resume/reactivation; disabled ID is discovered, never fixed");
}
