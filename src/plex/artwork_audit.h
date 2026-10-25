// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cache.h"
#include "client.h"
#include <fstream>
namespace plex {
inline std::string bounded_art_file(const std::string &path,size_t maximum){
    std::ifstream file(path,std::ios::binary|std::ios::ate);auto size=file?file.tellg():std::streampos(-1);
    if(size<=0 || static_cast<uint64_t>(size)>maximum)return {};
    std::string bytes(static_cast<size_t>(size),'\0');file.seekg(0);return file.read(&bytes[0],bytes.size())?bytes:std::string();
}
inline Result audit_installed_icon(const std::string &root){
    auto manifest=Json::parse(bounded_art_file(root+"/artwork-manifest.json",128*1024),nullptr,false);
    if(manifest.is_discarded() || !manifest.is_object() || text(manifest,"title_id")!="PPSA85301")return {{},"Artwork manifest is missing or belongs to another title; registration was not inspected",0};
    std::string expected;auto list=manifest.find("expected");
    if(list!=manifest.end() && list->is_object()){auto icon=list->find("sce_sys/icon0.png");if(icon!=list->end() && icon->is_object())expected=text(*icon,"sha256");}
    if(expected.size()!=64 || expected.find_first_not_of("0123456789abcdef")!=std::string::npos)return {{},"Artwork manifest has no valid expected icon checksum",0};
    auto bytes=bounded_art_file(root+"/sce_sys/icon0.png",4*1024*1024);if(bytes.empty())return {{},"Installed title icon is missing or unreadable; no cache diagnosis inferred",0};
    auto actual=cache_digest(bytes);if(actual.empty())return {{},"Icon checksum could not be calculated",0};
    return {Json{{"expected",expected},{"installed",actual},{"matches",expected==actual},{"registrationInspected",false}},"",200};
}
}
