#include "prefs.h"
#include <cassert>
#include <string>
static const char *directory;
extern "C" const char *plat_data_dir(){return directory;}
int main(int argc,char **argv){
    assert(argc==3); directory=argv[1];
    if(std::string(argv[2])=="write"){
        pref_set("plex_manual_uri",std::string("https://fixture.plex.direct:32401"));
        pref_set("plex_manual_server_id",std::string("fixture-id"));
        pref_set("plex_allow_http",0);
    } else {
        assert(pref_str("plex_manual_uri","")=="https://fixture.plex.direct:32401");
        assert(pref_str("plex_manual_server_id","")=="fixture-id");
        assert(pref_int("plex_allow_http",1)==0);
    }
}
