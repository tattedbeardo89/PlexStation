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
        pref_set("plex_preferred_server.id%3A42",std::string("server-one"));
        pref_set("plex_preferred_server.id%3A84",std::string("server-two"));
        pref_set("plex_controller_overlay",1);
    } else {
        assert(pref_str("plex_manual_uri","")=="https://fixture.plex.direct:32401");
        assert(pref_str("plex_manual_server_id","")=="fixture-id");
        assert(pref_int("plex_allow_http",1)==0);
        assert(pref_str("plex_preferred_server.id%3A42","")=="server-one");
        assert(pref_str("plex_preferred_server.id%3A84","")=="server-two");
        assert(pref_int("plex_controller_overlay",0)==1);
        pref_set("updated_application_setting",1);
        assert(pref_str("plex_preferred_server.id%3A42","")=="server-one");
    }
}
