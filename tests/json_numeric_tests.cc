// SPDX-License-Identifier: GPL-3.0-or-later
#include "plex/json_selftest.h"
#include <cassert>
#include <climits>
#include <cstring>
#ifdef EMPTY_DECIMAL
extern "C" struct lconv *__wrap_localeconv() {
    static char empty[]="";
    static struct lconv conventions{};
    conventions.decimal_point=empty;
    conventions.thousands_sep=empty;
    return &conventions;
}
#endif
int main() {
    using J=nlohmann::json;
    assert(plex::json_numeric_selftest());
    auto parse=[](const char *text){return J::parse(text,nullptr,false);};
    assert(parse("0")==0);
    assert(parse("-9223372036854775808").get<int64_t>()==INT64_MIN);
    assert(parse("9223372036854775807").get<int64_t>()==INT64_MAX);
    assert(parse("18446744073709551615").get<uint64_t>()==UINT64_MAX);
    for(const char *text:{"0.0","-0.0","0.5","-1.25","1e3","1E+3","1e-3","1.25e2","2.2250738585072014e-308","4.9406564584124654e-324","1.7976931348623157e308","18446744073709551616","-9223372036854775809"}) {
        const auto j=parse(text);
        assert(j.is_number());
        assert(!J::parse(j.dump(),nullptr,false).is_discarded());
    }
    for(const char *text:{"01","+1",".5","1.","1e","1e+","--1","NaN","Infinity","1e309","-1e309","[1.2.3]"})
        assert(parse(text).is_discarded());
    assert(parse("1e-9999")==0.0);
    puts("numeric JSON boundaries, malformed inputs, overflow and round trips passed");
}
