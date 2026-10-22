// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/search.h"
#include "plex/cloud.h"
#include "plex/music_feed.h"
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
 rows=home_rows(home_feed(aggregate,"https://pms.invalid","active-profile",{a,b}).data);assert(first==2 && second==2 && rows.size()==2 && rows[0].entries.size()==2 && rows[0].entries[0].id=="new");
 Client denied([](const Request &){return Reply{403,"private response must not be logged",""};},"fixture");assert(!cloud_watchlist(denied,"restricted"));
 Item music;music.id="music";music.type="artist";assert(home_rows(music_feed(denied,"https://pms.invalid","restricted",music,false).data).empty());
 puts("Grouped search, scoped cloud reads/writes/local resolution, filter encoding and multiple-library deduplication tests passed");
}
