// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/audio.h"
#include <cassert>
#include <iostream>
using namespace plex;
static Json track(int id,int disc=1,int index=1) {
 return {{"ratingKey",std::to_string(id)},{"type","track"},{"title","Track"},{"parentTitle","Album"},{"grandparentTitle","Artist"},{"parentIndex",disc},{"index",index},{"duration",180000},{"Media",Json::array({{{"container","flac"},{"audioCodec","flac"},{"Part",Json::array({{{"key","/library/parts/1/file.flac"}}})}}})}};
}
int main() {
 Json fixture;fixture["MediaContainer"]["Metadata"]=Json::array({track(1,2,1),track(2,1,2),track(3,1,1)});
 auto parsed=items(fixture);
 assert(parsed.size()==3 && parsed[0].artist=="Artist" && parsed[0].album=="Album" && parsed[0].disc==2);
 auto sorted=audio_tracks(fixture,true);
 assert(sorted[0].id=="3" && sorted[1].id=="2" && sorted[2].id=="1");
 for(const char *codec:{"mp3","aac","flac","alac","pcm_s16le","vorbis","opus"}) {
  auto item=parsed[0];item.media[0]["audioCodec"]=codec;
  assert(audio_direct_play("https://pms.invalid","profile-token",item).mode==Mode::DirectPlay);
 }
 auto invalid=parsed[0];invalid.media[0]["audioCodec"]="unknown";
 assert(audio_direct_play("https://pms.invalid","secret",invalid).mode==Mode::Unsupported);
 invalid=parsed[0];invalid.media[0]["Part"][0]["key"]="https://evil.invalid/file";
 assert(audio_direct_play("https://pms.invalid","secret",invalid).mode==Mode::Unsupported);
 invalid=parsed[0];invalid.type="movie";assert(audio_direct_play("https://pms.invalid","secret",invalid).mode==Mode::Unsupported);
 AudioQueue q;q.assign(sorted);assert(q.current()->id=="3");
 assert(q.add(parsed[0],true));assert(q.advance() && q.current()->id=="1");
 assert(!q.remove(q.cursor));assert(q.remove(0) && q.current()->id=="1");
 q.shuffle(42);auto current=q.current()->id;q.unshuffle();assert(q.current()->id==current);
 q.repeat=Repeat::One;auto cursor=q.cursor;assert(q.advance(true) && q.cursor==cursor);
 q.repeat=Repeat::All;q.cursor=q.order.size()-1;assert(q.advance() && q.cursor==0);
 q.repeat=Repeat::Off;q.cursor=q.order.size()-1;assert(!q.advance());q.clear();assert(!q.current() && !q.previous());
 int requests=0;
 Client client([&](const Request &r){
  assert(r.headers.at("X-Plex-Token")=="child-token");++requests;
  bool second=r.url.find("Start=200")!=std::string::npos;
  Json list=Json::array();for(int i=0;i<(second?3:200);++i)list.push_back(track((second?200:0)+i));
  return Reply{200,Json{{"MediaContainer",{{"Metadata",list},{"totalSize",203}}}}.dump(),""};
 },"fixture");
 auto full=audio_listing(client,"https://pms.invalid","child-token","/playlists/1/items",[]{return true;},true);
 assert(full && requests==2 && audio_tracks(full.data).size()==203);
 auto cancelled=audio_listing(client,"https://pms.invalid","child-token","/x",[]{return false;},true);assert(!cancelled && requests==2);
 Client huge([](const Request &){return Reply{200,"{\"MediaContainer\":{\"Metadata\":[],\"totalSize\":5000}}",""};},"fixture");
 assert(!audio_listing(huge,"https://pms.invalid","t","/x",[]{return true;},true));
 std::cout<<"Audio parsing, capabilities, multipart rejection, queue order/repeat/shuffle and paginated scoped API tests passed\n";
}
