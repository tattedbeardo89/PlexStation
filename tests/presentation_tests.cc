// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/presentation.h"
#include "plex/image_source.h"
#include "plex/home_feed.h"
#include "plex/navigation.h"
#include "plex/artwork_audit.h"
#include <filesystem>
#include <cassert>
#include <iostream>
using namespace plex;
int main(){
 Item movie;movie.type="movie";Item show=movie;show.type="show";Item episode=movie;episode.type="episode";Item album=movie;album.type="album";Item category=movie;category.type="category";
 assert(card_kind(movie)==CardKind::Poster && card_kind(show)==CardKind::Poster && card_kind(episode)==CardKind::Episode);
 assert(card_kind(album)==CardKind::Square && card_kind(album,true)==CardKind::Book && card_kind(category)==CardKind::Category);
 for(bool grid:{false,true})for(auto kind:{CardKind::Poster,CardKind::Episode,CardKind::Book,CardKind::Square,CardKind::Category}){
  auto size=card_size(kind,grid);auto target=(kind==CardKind::Poster || kind==CardKind::Book)?2.f/3:kind==CardKind::Square?1.f:16.f/9;
  assert(std::abs(size.width/size.height-target)<.001f);
 }
 std::vector<CardKind> kinds;for(int i=0;i<30;++i)kinds.push_back(i%3==0?CardKind::Episode:i%3==1?CardKind::Poster:CardKind::Square);
 auto boxes=card_flow(kinds);for(size_t i=0;i<boxes.size();++i){assert(boxes[i].x>=0 && boxes[i].x+boxes[i].width<=1440);if(i && boxes[i].row==boxes[i-1].row)assert(boxes[i].x>=boxes[i-1].x+boxes[i-1].width+24);}
 for(int selected:{0,15,29}){float offset=rail_offset(kinds,selected,0);float x=0;for(int i=0;i<selected;++i)x+=card_size(kinds[i]).width+24;assert(x-offset>=-.01 && x+card_size(kinds[selected]).width-offset<=1440.01);}
 assert(grid_neighbor(boxes,0,-1)==0 && boxes[grid_neighbor(boxes,0,1)].row==1);
 episode.thumb="/episode";episode.art="/series-art";episode.parent_thumb="/season";episode.grandparent_thumb="/show";episode.grandparent_art="/backdrop";
 auto sources=image_candidates(episode,true);assert(sources.front()=="/episode" && sources.back()=="/show");
 episode.thumb=episode.parent_thumb;sources=image_candidates(episode,true);assert(sources.front()=="/series-art");
 assert(initials("Alice Beardo")=="AB" && initials("")=="?" && category_art("Genre: Science Fiction")=="science-fiction" && category_art("Unknown category")=="other");
 Request q;std::string key;
 assert(image_request("https://server.invalid","private","/library/a?ts=1",false,q,key) && q.headers.at("X-Plex-Token")=="private");
 assert(image_request("https://server.invalid","private","https://metadata-static.plex.tv/image?width=320&X-Plex-Token=secret",false,q,key));
 assert(q.headers.count("X-Plex-Token")==0 && key=="https://metadata-static.plex.tv/image?width=320");
 assert(!image_request("https://server.invalid","private","https://unknown.invalid/image",true,q,key));
 image_request("https://server.invalid","private","/poster",true,q,key);auto original=q;
 assert(image_redirect(original,q,"https://images.plex.tv/photo?token=secret") && q.headers.count("X-Plex-Token")==0);
 assert(!image_redirect(original,q,"http://server.invalid/image") && !image_redirect(original,q,"https://evil.invalid/image"));
 assert(!released_date("2999-01-01"));
 assert(release_date("2024-02-29") && !release_date("2023-02-29") && !release_date("") && !release_date("added-yesterday"));
 Json hub={{"hubIdentifier","movie.recently_added"},{"type","movie"}};assert(hub_semantic(hub)=="recentlyAddedMovies");
 hub["type"]="episode";assert(hub_semantic(hub)=="recentlyAddedEpisodes");
 Item section;section.id="1";section.type="movie";section.title="Movies";int incremental=0;
 Client client([](const Request &r){
  if(r.url.find("/hubs/continueWatching")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"watch","type":"episode","title":"Episode","thumb":"/still","grandparentThumb":"/show","grandparentArt":"/background"}]}})",""};
  if(r.url.find("/hubs")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"hubIdentifier":"movie.recentlyadded","type":"movie","Metadata":[{"ratingKey":"a","type":"movie","title":"New","addedAt":900}]},{"hubIdentifier":"movie.recently_added","type":"movie","Metadata":[{"ratingKey":"a","type":"movie","title":"New","addedAt":900}]}]}})",""};
  return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
 },"fixture");
 auto feed=home_feed(client,"https://server.invalid","profile",{section},[]{return true;},[&](const Json &snapshot){++incremental;assert(!home_rows(snapshot).empty());});
 auto rows=home_rows(feed.data);int added=0;for(const auto &row:rows)if(row.id=="recentlyAddedMovies")++added;
 assert(added==1 && incremental>0 && rows[0].entries[0].grandparent_thumb=="/show");
 Client tv([](const Request &r){
  if(r.url.find("/hubs/sections/")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"hubIdentifier":"tv.recently_added","type":"show","Metadata":[{"ratingKey":"series","type":"show","title":"Series"}]},{"hubIdentifier":"tv.recently_released","type":"episode","Metadata":[{"ratingKey":"old","type":"episode","title":"Imported older","addedAt":9999,"originallyAvailableAt":"1990-01-01"},{"ratingKey":"new","type":"episode","title":"New episode","addedAt":1,"originallyAvailableAt":"2026-01-01"}]}]}})",""};
  if(r.url.find("originallyAvailableAt")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"new","type":"episode","title":"New episode","addedAt":1,"originallyAvailableAt":"2026-01-01"},{"ratingKey":"old","type":"episode","title":"Imported older","addedAt":9999,"originallyAvailableAt":"1990-01-01"},{"ratingKey":"unknown","type":"episode","title":"No air date","addedAt":10000}]}})",""};
  if(r.url.find("recentlyAdded?type=2")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"series","type":"show","title":"Series"},{"ratingKey":"not-series","type":"episode","title":"Imported"}]}})",""};
  return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};
 },"fixture");
 section.type="show";rows=home_rows(library_feed(tv,"https://server.invalid","profile",section,false).data);int released=0;added=0;
 for(const auto &row:rows){if(row.id=="recentlyAddedTV"){++added;assert(row.entries.size()==1 && row.entries[0].type=="show");}if(row.id=="recentlyReleasedTV"){++released;assert(row.entries.size()==2 && row.entries[0].id=="new" && row.entries[1].id=="old");}}
 assert(added==1 && released==1);
 Json members={{"users",Json::array({{{"id",1},{"title","Admin"},{"admin",true},{"protected",false},{"thumb","https://plex.tv/users/admin/avatar"}},{{"id",2},{"title","Managed"},{"restricted",true},{"protected",false},{"thumb","/users/managed/avatar"}},{{"id",3},{"title","PIN User"},{"protected",true}},{{"id",4},{"title","Guest"},{"protected",false}}})}};
 auto people=profiles(members);assert(people.size()==4 && people[0].admin && people[1].managed && people[2].protected_user && people[3].avatar.empty());
 Item token_thumb;token_thumb.thumb="https://metadata-static.plex.tv/image?X-Plex-Token=secret&width=320";assert(text(item_json(token_thumb),"thumb")=="https://metadata-static.plex.tv/image?width=320");
 Client scoped_hubs([](const Request &r){if(r.url.find("/hubs?")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[{"hubIdentifier":"movie.recently_added","type":"movie","Metadata":[{"ratingKey":"one","type":"movie","title":"One","librarySectionID":1}]},{"hubIdentifier":"movie.recently_added","type":"movie","Metadata":[{"ratingKey":"two","type":"movie","title":"Two","librarySectionID":2}]}]}})",""};return Reply{200,R"({"MediaContainer":{"Metadata":[]}})",""};},"fixture");
 Item second_section=section;section.type=second_section.type="movie";second_section.id="2";
 auto scoped_rows=home_rows(home_feed(scoped_hubs,"https://server.invalid","profile",{section,second_section}).data);assert(scoped_rows.size()==2 && scoped_rows[0].id!=scoped_rows[1].id);
 auto audit_root=std::filesystem::temp_directory_path()/"plex-icon-audit-fixture";std::filesystem::remove_all(audit_root);std::filesystem::create_directories(audit_root/"sce_sys");
 {std::ofstream icon(audit_root/"sce_sys/icon0.png");icon<<"fixture image";std::ofstream manifest(audit_root/"artwork-manifest.json");manifest<<Json{{"title_id","PPSA85301"},{"expected",{{"sce_sys/icon0.png",{{"sha256",cache_digest("fixture image")}}}}}}.dump();}
 auto audit=audit_installed_icon(audit_root.string());assert(audit && flag(audit.data,"matches") && !flag(audit.data,"registrationInspected"));
 {std::ofstream icon(audit_root/"sce_sys/icon0.png");icon<<"different staged image";}
 assert(!flag(audit_installed_icon(audit_root.string()).data,"matches"));std::filesystem::remove_all(audit_root);
 std::cout<<"Presentation: exact aspect ratios, first/middle/last rails, grid bounds, metadata fallback, scoped URLs, incremental Home and semantic dedup passed\n";
}
