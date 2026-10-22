// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/streaming.h"
#include "plex/progress_outbox.h"
#include "plex/episodes.h"
#include <cassert>
#include <cstdio>
using namespace plex;
int main(){
 Item item;item.id="42";item.type="movie";item.media=Json::array({{{"container","avi"},{"videoCodec","h264"},{"Part",Json::array({{{"key","/library/parts/42/file.avi"}}})}}});
 std::string decision="copy";int requests=0;
 Client client([&](const Request &r){
  ++requests;assert(r.headers.at("X-Plex-Token")=="profile-pms");
  assert(r.url.find("/video/:/transcode/universal/decision?")!=std::string::npos && r.url.find("maxVideoBitrate=8000")!=std::string::npos);
  Json out;out["MediaContainer"]["generalDecisionCode"]=1001;
  out["MediaContainer"]["Metadata"]=Json::array({{{"ratingKey","42"},{"Media",Json::array({{{"videoCodec","h264"},{"Part",Json::array({{{"Stream",Json::array({{{"streamType",1},{"codec","h264"},{"decision",decision}},{{"streamType",2},{"decision","copy"}}})}}})}}})}}});
  return Reply{200,out.dump(),""};
 },"fixture");
 auto stream=negotiate_video(client,"https://pms.invalid","profile-pms",item,0,Quality::FHD,false,"session-1");
 assert(stream.playback.mode==Mode::DirectStream && stream.session=="session-1" && stream.playback.url.find("start.m3u8")!=std::string::npos);
 decision="transcode";stream=negotiate_video(client,"https://pms.invalid","profile-pms",item,0,Quality::FHD,false,"session-2");assert(stream.playback.mode==Mode::Transcode);
 decision="unknown";stream=negotiate_video(client,"https://pms.invalid","profile-pms",item,0,Quality::FHD,false,"session-3");assert(stream.playback.mode==Mode::Unsupported);
 item.media[0]["container"]="mp4";auto direct=negotiate_video(client,"https://pms.invalid","profile-pms",item,0,Quality::Original,true,"s");assert(direct.playback.mode==Mode::DirectPlay && requests==3);
 auto query=universal_query(item,0,"s",Quality::Automatic,true);assert(query.find("maxVideoBitrate")==std::string::npos);
 query=universal_query(item,0,"s",Quality::Automatic,false);assert(query.find("8000")!=std::string::npos);
 Json subtitle={{"streamType",3},{"codec","srt"},{"key","/library/streams/99"}};
 assert(external_subtitle("https://pms.invalid","profile-pms",subtitle)=="https://pms.invalid/library/streams/99?X-Plex-Token=profile-pms");
 subtitle["key"]="https://evil.invalid/";assert(external_subtitle("https://pms.invalid","t",subtitle).empty());
 Client xml([](const Request &){return Reply{200,R"(<MediaContainer generalDecisionCode="1001"><Video ratingKey="42"><Media videoCodec="h264"><Part><Stream streamType="1" codec="h264" decision="copy"/></Part></Media></Video></MediaContainer>)",""};},"fixture");
 item.media[0]["container"]="avi";assert(negotiate_video(xml,"https://pms.invalid","t",item,0,Quality::Original,true,"s").playback.mode==Mode::DirectStream);
 Item audio;audio.id="11";audio.type="track";audio.media=item.media;assert(audio_transcode("https://pms.invalid","profile-pms",audio,"s").mode==Mode::Transcode);
 ProgressOutbox box;item.id="movie1";auto old=box.put(item,"playing",100,1000);auto fresh=box.put(item,"paused",200,1001);box.ack(item.id,old);assert(box.entries.size()==1 && box.entries[0].position==200);box.failed(item.id,old,10);assert(box.entries[0].attempts==0);box.failed(item.id,fresh,10);assert(box.entries[0].due==20);
 ProgressOutbox reboot;reboot.restore(box.json(),30);assert(reboot.entries.size()==1 && reboot.entries[0].due==35);reboot.ack(item.id,reboot.entries[0].sequence);assert(reboot.entries.empty());
 Item episode;episode.id="special";episode.type="episode";episode.grandparent_id="show";
 Client series([](const Request &q){assert(q.headers.at("X-Plex-Token")=="child");return Reply{200,R"({"MediaContainer":{"totalSize":3,"Metadata":[{"ratingKey":"ep1","type":"episode"},{"ratingKey":"special","type":"episode","parentIndex":0,"index":1},{"ratingKey":"ep2","type":"episode"}]}})",""};},"fixture");
 auto next=next_episode(series,"https://pms.invalid","child",episode);assert(next && items(next.data)[0].id=="ep2");
 puts("Direct Play/Stream/Transcode distinction, JSON/XML decision, quality policy, scoped token, subtitle origin and audio fallback tests passed");
}
