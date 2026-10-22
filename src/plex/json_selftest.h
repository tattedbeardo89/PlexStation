// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nlohmann/json.hpp"
#include <cstdio>
#include <cstdlib>
#include <locale.h>
namespace plex {
inline bool json_numeric_selftest() {
    using J=nlohmann::json;
    const auto j=J::parse("[-9223372036854775808,18446744073709551615,0.5,-1.25,1.25e2,1e-3]",nullptr,false);
    const bool ok=j.is_array() && j.size()==6 && j[0].is_number_integer() &&
        j[1].is_number_unsigned() && j[2]==0.5 && j[3]==-1.25 && j[4]==125.0 && j[5]==0.001;
    const auto *loc=localeconv();
    char *end=nullptr;
    const double value=std::strtod("0.5",&end);
    std::fprintf(stderr,"json: numeric selftest=%s locale_separator=%s dot_conversion=%s\n",
        ok?"pass":"fail",!loc || !loc->decimal_point?"null":loc->decimal_point[0]?"present":"empty",
        value==0.5 && end && *end=='\0'?"pass":"fail");
    std::fflush(stderr);
    return ok;
}
}
