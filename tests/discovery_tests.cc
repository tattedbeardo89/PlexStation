// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/search.h"
#include "plex/cloud.h"
#include "plex/music_feed.h"
#include "plex/navigation.h"
#include "plex/live_tv.h"
#include <cassert>
#include <cstdio>
using namespace plex;
int main(){
 int cloud_calls=0;
 Client client([&](const Request &q){
  assert(q.headers.at("X-Plex-Token")=="active-profile");
  if(q.url.find("discover.provider.plex.tv")!=std::string::npos){++cloud_calls;if(q.method=="PUT"){assert(q.url.find("ratingKey=abcdef")!=std::string::npos);return Reply{204,"",""};}return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"abcdef","guid":"plex://movie/abcdef","type":"movie","title":"Cloud film"}]}})",""};}
  if(q.url.find("/hubs/search?")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"type":"movie","title":"Movies","Metadata":[{"ratingKey":"1","type":"movie","title":"Film"}]},{"type":"track","title":"Tracks","Metadata":[{"ratingKey":"2","type":"track","title":"Song","librarySectionID":"music"}]},{"type":"show","title":"Empty","Metadata":[]}]}})",""};
  assert(q.url.find("/library/all?guid=plex%3A%2F%2Fmovie%2Fabcdef&type=1")!=std::string::npos);
  return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"local-42","type":"movie","title":"Authorized local film"}]}})",""};
 },"fixture");
 auto cloud=cloud_watchlist(client,"active-profile");auto entries=items(cloud.data,false);assert(entries.size()==1 && cloud_id(entries[0])=="abcdef");
 assert(cloud_change(client,"active-profile",entries[0],true));assert(cloud_change(client,"active-profile",entries[0],false));
 auto local=resolve_cloud(client,"https://pms.invalid","active-profile",entries[0]);assert(local && items(local.data)[0].id=="local-42");
 entries[0].guid="plex://movie/../../evil";assert(!cloud_change(client,"active-profile",entries[0],true));assert(cloud_calls==3);
 auto search=global_search(client,"https://pms.invalid","active-profile","Film & Song");auto rows=home_rows(search.data);assert(rows.size()==2 && rows[1].entries[0].type=="track" && rows[1].entries[0].section_id=="music");
 assert(library_query("/library/sections/1/all?old=1",{{"genre","A&B"},{"year","2020"}},"duration:asc")=="/library/sections/1/all?sort=duration%3Aasc&genre=A%26B&year=2020");
 int first=0,second=0;
 Client aggregate([&](const Request &q){
  if(q.url.find("/library/sections/1/")!=std::string::npos){++first;return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"same","type":"movie","title":"Duplicate","addedAt":1}]}})",""};}
  if(q.url.find("/library/sections/2/")!=std::string::npos){++second;return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"new","type":"movie","title":"Newer","addedAt":2},{"ratingKey":"same","type":"movie","title":"Duplicate","addedAt":1}]}})",""};}
  return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
 },"fixture");
 Item a,b;a.id="1";b.id="2";a.type=b.type="movie";
 rows=home_rows(home_feed(aggregate,"https://pms.invalid","active-profile",{a,b}).data);assert(first==3 && second==3 && rows.size()==2 && std::any_of(rows.begin(),rows.end(),[](const Hub &h){return h.id=="recentlyAddedMovies" && h.entries.size()==2 && h.entries[0].id=="new";}));
 Client denied([](const Request &){return Reply{403,"private response must not be logged",""};},"fixture");assert(!cloud_watchlist(denied,"restricted"));
 Item music;music.id="music";music.type="artist";assert(home_rows(music_feed(denied,"https://pms.invalid","restricted",music,false).data).empty());
 Item home;a.title="Movies";home.id="personal";home.title="Home Movies";home.type="movie";
 assert(default_library({a,b,home},1,"missing").id==a.id);
 assert(default_library({a,b,home},1,b.id).id==b.id);
 assert(default_library({a,b,home},9,"missing").id=="personal");
 assert(default_library({a,b,home},2,"missing").id.empty());
 home.title="Family Videos";home.personal=true;assert(default_library({a,b,home},9,"personal").id=="personal");
 Client scoped([](const Request &q){
   assert(q.url.find("/library/sections/allowed/")!=std::string::npos || q.url.find("/hubs/sections/allowed?")!=std::string::npos);
   if(q.url.find("/genre")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Directory":[{"key":"horror","title":"Horror"}]}})",""};
   if(q.url.find("/hubs")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[]}})",""};
   return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"real","type":"movie","title":"Fixture returned by server"}]}})",""};
 },"fixture");
 Item allowed;allowed.id="allowed";allowed.type="movie";
 auto categories=items(library_categories(scoped,"https://pms.invalid","active-profile",allowed).data,false);
 assert(categories.size()==1 && categories[0].type=="category" && categories[0].key=="/library/sections/allowed/all?genre=horror");
 auto seasonal=home_rows(library_feed(scoped,"https://pms.invalid","active-profile",allowed,true,10).data);
 assert(std::any_of(seasonal.begin(),seasonal.end(),[](const Hub &h){return h.id=="seasonal" && h.entries[0].id=="real";}));
 auto disabled=home_rows(library_feed(scoped,"https://pms.invalid","active-profile",allowed,false,10).data);
 assert(std::none_of(disabled.begin(),disabled.end(),[](const Hub &h){return h.id=="seasonal";}));
 Result seasonal_home{Json{{"rows",Json::array()}},"",200};
 append_seasonal_home(seasonal_home,scoped,"https://pms.invalid","active-profile",{allowed},10,[]{return true;});
 assert(home_rows(seasonal_home.data).size()==1 && home_rows(seasonal_home.data)[0].entries[0].id=="real");
 Result cancelled_home{Json{{"rows",Json::array()}},"",200};
 append_seasonal_home(cancelled_home,scoped,"https://pms.invalid","active-profile",{allowed},10,[]{return false;});assert(home_rows(cancelled_home.data).empty());
 assert(seasonal_tag("Winter favorites",1) && seasonal_tag("Summer collection",7));
 assert(!seasonal_tag("Horror",7) && !seasonal_tag("Summer",3));
 int personal_calls=0;
 Client exclusions([&](const Request &q){if(q.url.find("/sections/personal/")!=std::string::npos)++personal_calls;return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};},"fixture");
 home_feed(exclusions,"https://pms.invalid","active-profile",{home,allowed});assert(personal_calls==0);
 Client seasons([](const Request &q){
   if(q.url.find("/genre")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Directory":[]}})",""};
   if(q.url.find("/collections?")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"winter","type":"collection","title":"Winter Favorites"}]}})",""};
   assert(q.url.find("/library/collections/winter/children?")!=std::string::npos);
   return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"snow","type":"movie","title":"Snow"}]}})",""};
 },"fixture");
 auto winter=seasonal_library(seasons,"https://pms.invalid","active-profile",allowed,1);assert(winter && items(winter.data)[0].id=="snow");
 auto winter_categories=items(library_categories(seasons,"https://pms.invalid","active-profile",allowed).data);assert(winter_categories.size()==1 && winter_categories[0].type=="collection");
 int live_calls=0;
 Client live([&](const Request &q){
  ++live_calls;assert(q.headers.at("X-Plex-Token")=="profile");
  if(q.url.find("/livetv/dvrs/")!=std::string::npos){assert(q.method=="POST" && q.url.find("/livetv/dvrs/12/channels/channel/tune")!=std::string::npos);return Reply{200,R"({"MediaContainer":{"Metadata":[{"live":true,"Media":[{"uuid":"session-id","Part":[{"Stream":[{"streamType":1,"codec":"h264"}]}]}]}]}})",""};}
  if(q.url.find("/livetv/dvrs")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Dvr":[{"key":"12"}]}})",""};
  if(q.url.find("/media/providers")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"MediaProvider":[{"identifier":"tv.plex.providers.epg.cloud:12"},{"identifier":"tv.plex.providers.epg.cloud:999"},{"identifier":"https://evil.invalid"}]}})",""};
  if(q.url.find("/lineups/dvr/channels")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Channel":[{"id":"channel","gridKey":"grid","title":"Real channel"}]}})",""};
  assert(q.url.find("/tv.plex.providers.epg.cloud:12/grid?channelGridKey=grid&date=2026-10-23")!=std::string::npos);
  return Reply{200,R"({"MediaContainer":{"Metadata":[{"title":"Real program","Media":[{"beginsAt":100,"endsAt":200}]}]}})",""};
 },"fixture");
 auto channels=live_channels(live,"https://pms.invalid","profile");assert(channels && channels.data["channels"].size()==1 && live_calls==3);
 auto channel=channels.data["channels"][0];auto guide=live_guide(live,"https://pms.invalid","profile",channel,"2026-10-23");assert(guide && guide.data["programs"][0]["title"]=="Real program");
 auto tuned=live_tune(live,"https://pms.invalid","profile",channel,"device");assert(tuned && text(tuned.data,"url").find("/livetv/sessions/session-id/device/index.m3u8?")!=std::string::npos);
 assert(!live_provider("tv.plex.providers.epg.cloud:12/evil").size());
 assert(!live_guide(live,"https://pms.invalid","profile",channel,"invalid"));
 assert(!live_channels(denied,"https://pms.invalid","restricted"));
 Client protected_tv([](const Request &){return Reply{200,R"({"MediaContainer":{"Metadata":[{"drm":true,"Media":[{"uuid":"session-id"}]}]}})",""};},"fixture");
 assert(!live_tune(protected_tv,"https://pms.invalid","profile",channel,"device"));
 Client audio_empty([](const Request &){return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};},"fixture");
 Item owned_book;owned_book.id="book";owned_book.type="album";owned_book.section_id="music";Item other_book=owned_book;other_book.id="other";other_book.section_id="other-library";
 auto book_rows=home_rows(music_feed(audio_empty,"https://pms.invalid","profile",music,true,{owned_book,other_book}).data);
 assert(book_rows.size()==1 && book_rows[0].entries.size()==1 && book_rows[0].entries[0].id=="book");
 int recording_deletes=0;
 Client recordings([&](const Request &q){
  assert(q.headers.at("X-Plex-Token")=="authorized-owner");
  if(q.method=="DELETE"){++recording_deletes;assert(q.url=="https://pms.invalid/media/subscriptions/77");return Reply{204,"",""};}
  if(q.url.find("/scheduled")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"MediaGrabOperation":[{"status":"scheduled","Metadata":{"title":"Real episode","grandparentTitle":"Series","Media":[{"beginsAt":1234}]}}]}})",""};
  assert(q.url.find("/media/subscriptions?includeGrabs=1&includeStorage=1")!=std::string::npos);
  return Reply{200,R"({"MediaContainer":{"MediaSubscription":[{"key":"77","type":2,"targetLibrarySectionID":1,"title":"Record Series"},{"key":"evil/../77","type":2,"targetLibrarySectionID":1},{"key":"88","type":15}]}})",""};
 },"fixture");
 auto rules=live_recordings(recordings,"https://pms.invalid","authorized-owner");
 assert(rules && rules.data["subscriptions"].size()==1 && rules.data["scheduled"].size()==1 && recording_deletes==0);
 assert(!cancel_recording_rule(recordings,"https://pms.invalid","authorized-owner","stale") && recording_deletes==0);
 assert(cancel_recording_rule(recordings,"https://pms.invalid","authorized-owner","77") && recording_deletes==1);
 assert(!live_recordings(denied,"https://pms.invalid","restricted"));
 Client malformed_rules([](const Request &){return Reply{200,R"({"MediaContainer":{"MediaSubscription":42}})",""};},"fixture");
 assert(!live_recordings(malformed_rules,"https://pms.invalid","restricted"));
 puts("Grouped search, scoped cloud reads/writes/local resolution, filter encoding and multiple-library deduplication tests passed");
}
