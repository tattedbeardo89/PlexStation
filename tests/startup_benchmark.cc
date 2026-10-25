// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/home_feed.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cassert>
using namespace plex;
int main(){
 int requests=0;auto begin=std::chrono::steady_clock::now();double first=-1;
 Client client([&](const Request &q){++requests;std::this_thread::sleep_for(std::chrono::milliseconds(10));
  if(q.url.find("/continueWatching")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"watch","type":"episode","title":"Watching"}]}})",""};
  if(q.url.find("/hubs?")!=std::string::npos)return Reply{200,R"({"MediaContainer":{"Hub":[]}})",""};
  return Reply{200,R"({"MediaContainer":{"Metadata":[{"ratingKey":"film","type":"movie","title":"Film","originallyAvailableAt":"2020-01-01"}]}})",""};
 },"benchmark");
 std::vector<Item> sections;for(int i=0;i<4;++i){Item s;s.id=std::to_string(i+1);s.type=i<2?"movie":"show";sections.push_back(s);}
#ifdef BASELINE_FEED
 auto result=home_feed(client,"https://fixture.invalid","profile",sections);
 first=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 const char *mode="baseline-complete-only";
#else
 auto result=home_feed(client,"https://fixture.invalid","profile",sections,[]{return true;},[&](const Json &data){if(first<0 && !home_rows(data).empty())first=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();});
 const char *mode="incremental";
#endif
 assert(result && first>=0);
 double total=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 printf("{\"mode\":\"%s\",\"first_usable_metadata_ms\":%.3f,\"complete_metadata_ms\":%.3f,\"requests\":%d,\"controlled_delay_per_request_ms\":10,\"scope\":\"mock transport; not PS5 or live Plex latency\"}\n",mode,first,total,requests);
}
