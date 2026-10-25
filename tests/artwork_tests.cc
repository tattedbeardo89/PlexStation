// Production decoding/cache lifecycle with fake transport and GPU upload only.
#include "plex/artwork.h"
#include "plex/diagnostics.h"
#include "library.h"
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "stb_image.h"
#pragma GCC diagnostic pop
#include <cassert>
#include "avatar_image_fixtures.h"
#include <chrono>
#include <thread>
#include <filesystem>
static int uploaded,freed,last_width,last_height;
ImTextureID library_upload_rgba(const unsigned char *rgba,int w,int h) {
    assert(rgba && w>0 && h>0 && w<=640 && h<=960);last_width=w;last_height=h;return ++uploaded;
}
void library_free_image(ImTextureID texture) {if(texture) ++freed;}
static void settle(plex::Artwork &art) {
    for(int i=0;i<1500 && art.busy();++i) {std::this_thread::sleep_for(std::chrono::milliseconds(2));art.update();}
    assert(!art.busy());
}
int main() {
    // One-pixel uncompressed TGA, real stb decode.
    std::string image(18,'\0'); image[2]=2; image[12]=image[14]=1; image[16]=24;
    image.append(3,'\x7f');
    plex::Artwork art([&](const plex::Request &q){assert(q.headers.at("X-Plex-Token")=="fixture");return plex::Reply{200,image,""};});
    assert(!art.get("https://fixture.invalid","fixture","//invalid"));
    assert(!art.get("https://fixture.invalid","fixture","/poster")); settle(art);
    assert(art.get("https://fixture.invalid","fixture","/poster") && uploaded==1);
    // Clearing during decode discards the obsolete generation without upload.
    art.get("https://fixture.invalid","fixture","/next"); art.clear(); settle(art);
    assert(uploaded==1 && freed==1);
    image[12]=static_cast<char>(641 & 255); image[13]=static_cast<char>(641 >> 8);
    art.get("https://fixture.invalid","fixture","/oversized"); settle(art); assert(uploaded==1);
    image="corrupt";
    art.get("https://fixture.invalid","fixture","/corrupt"); settle(art); assert(uploaded==1);
    image.assign(18,'\0'); image[2]=2; image[12]=image[14]=1; image[16]=24; image.append(3,'\x7f');
    for(int i=0;i<14;++i) {art.get("https://fixture.invalid","fixture","/"+std::to_string(i));settle(art);}
    assert(uploaded==15 && freed==1); art.clear();assert(freed==1);
    art.update();assert(freed==uploaded); // current-frame references survive until the next frame
    plex::diagnostic_flags=plex::NoDownloads; assert(!art.get("https://fixture.invalid","fixture","/no-download")); settle(art); assert(uploaded==15);
    plex::diagnostic_flags=plex::NoDecode; art.get("https://fixture.invalid","fixture","/no-decode");settle(art);assert(uploaded==15);
    plex::diagnostic_flags=plex::NoUpload; art.get("https://fixture.invalid","fixture","/no-upload");settle(art);assert(uploaded==15);
    plex::diagnostic_flags=0; art.clear();
    assert(!art.avatar("https://evil.invalid/avatar","fixture"));
    assert(!art.avatar("http://evil.invalid/avatar","fixture"));
    art.avatar("https://plex.tv/users/fixture/avatar","fixture");settle(art);assert(uploaded==16);auto before_clear=freed;art.clear();assert(freed==before_clear);art.update();assert(freed==uploaded);
    int requests=0;
    plex::Artwork redirected([&](const plex::Request &q){
        ++requests;
        if(q.url=="https://plex.tv/users/avatar") {
            assert(q.headers.at("X-Plex-Token")=="private-fixture");
            plex::Reply reply{302,"",""};reply.location="https://images.plex.tv/avatar?X-Plex-Token=private-fixture";return reply;
        }
        assert(q.url=="https://images.plex.tv/avatar" && q.headers.count("X-Plex-Token")==0);
        return plex::Reply{200,image,""};
    });
    redirected.avatar("/users/avatar","private-fixture");settle(redirected);
    auto avatar_texture=redirected.avatar("/users/avatar","private-fixture");
    assert(avatar_texture && requests==2 && redirected.aspect(avatar_texture)==1);
    redirected.clear();redirected.update();
    requests=0;
    plex::Artwork rejected([&](const plex::Request &){++requests;plex::Reply r{302,"",""};r.location="https://evil.invalid/avatar";return r;});
    rejected.avatar("/users/avatar","private-fixture");settle(rejected);assert(requests==1);
    // Failed entries have a bounded retry delay, rather than permanent negative caching.
    rejected.avatar("/users/avatar","private-fixture");assert(requests==1);
    for(int i=0;i<301;++i)rejected.update();
    rejected.avatar("/users/avatar","private-fixture");settle(rejected);assert(requests==2);
    assert(!rejected.avatar("", "fixture"));
    assert(!rejected.avatar("//evil.invalid/avatar", "fixture"));
    for(int failure=0;failure<3;++failure) {
        int calls=0;
        plex::Artwork unavailable([&](const plex::Request &){++calls;return failure==0?plex::Reply{401,"",""}:failure==1?plex::Reply{0,"","Timeout"}:plex::Reply{200,"invalid image",""};});
        unavailable.avatar("/users/avatar","fixture");settle(unavailable);
        assert(!unavailable.avatar("/users/avatar","fixture") && calls==1);
        unavailable.clear();unavailable.update();
    }
    // A legitimate large square avatar is bounded to a 256x256 upload.
    image.assign(18,'\0');image[2]=2;image[12]=image[14]=0;image[13]=image[15]=4;image[16]=24;
    image.append(1024*1024*3,'\x7f');
    plex::Artwork large([&](const plex::Request &){return plex::Reply{200,image,""};});
    large.avatar("https://images.plex.tv/large","fixture");settle(large);
    assert(large.avatar("https://images.plex.tv/large","fixture") && last_width==256 && last_height==256);large.clear();large.update();
    for(int format=0;format<2;++format) {
        const auto *bytes=format?fixture_jpeg:fixture_png;
        const size_t size=format?sizeof(fixture_jpeg):sizeof(fixture_png);
        image.assign(reinterpret_cast<const char *>(bytes),size);
        large.avatar("https://images.plex.tv/fixture/"+std::to_string(format),"fixture");settle(large);
        assert(large.avatar("https://images.plex.tv/fixture/"+std::to_string(format),"fixture"));
        large.clear();large.update();
    }
    image.assign(18,'\0');image[2]=2;image[12]=2;image[14]=1;image[16]=24;image.append(6,'\x7f');
    large.landscape("https://fixture.invalid","fixture","/wide");settle(large);
    auto wide=large.landscape("https://fixture.invalid","fixture","/wide");
    assert(wide && large.aspect(wide)==2);large.clear();assert(large.aspect(wide)==0);large.update();
    // Real disk restart: no second network request, endpoint changes retain identity.
    char temporary[]="/tmp/plex-artwork-tests-XXXXXX";assert(mkdtemp(temporary));
    int disk_requests=0;
    auto network=[&](const plex::Request &){++disk_requests;return plex::Reply{200,image,""};};
    {
        plex::Artwork first(network);first.set_scope(temporary,"account/profile/server");
        first.update();settle(first);
        first.get("https://fixture.invalid","fixture","/persist");settle(first);
        assert(first.get("https://fixture.invalid","fixture","/persist"));
        first.clear();first.update();
    }
    assert(disk_requests==1);
    {
        plex::Artwork restart(network);restart.set_scope(temporary,"account/profile/server");
        restart.update();settle(restart);
        restart.get("https://different.invalid","fixture","/persist");settle(restart);
        assert(restart.get("https://different.invalid","fixture","/persist") && disk_requests==1 && restart.disk_hits()==1);
        restart.clear();restart.clear_disk();restart.update();settle(restart);
        restart.get("https://fixture.invalid","fixture","/persist");settle(restart);assert(disk_requests==2);
        restart.clear();restart.update();
    }
    // Corrupt disk bytes are rejected by the production decoder, then refetched.
    plex::DiskCache corrupt(temporary,"account/profile/server");assert(corrupt.put("poster:/corrupt","not an image"));
    {
        plex::Artwork recovered(network);recovered.set_scope(temporary,"account/profile/server");recovered.update();settle(recovered);
        recovered.get("https://fixture.invalid","fixture","/corrupt");settle(recovered);
        assert(recovered.get("https://fixture.invalid","fixture","/corrupt") && disk_requests==3);
        recovered.clear();recovered.update();
    }
    // A completed fetch must wait rather than evict any currently visible texture.
    plex::Artwork visible(network);
    for(int i=0;i<64;++i){visible.get("https://fixture.invalid","fixture","/visible/"+std::to_string(i));settle(visible);}
    for(int i=0;i<64;++i)assert(visible.get("https://fixture.invalid","fixture","/visible/"+std::to_string(i)));
    int visible_uploads=uploaded;
    visible.get("https://fixture.invalid","fixture","/extra");
    for(int frame=0;frame<20;++frame){
        for(int i=0;i<64;++i)assert(visible.get("https://fixture.invalid","fixture","/visible/"+std::to_string(i)));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));visible.update();
    }
    assert(uploaded==visible_uploads && visible.busy());
    settle(visible);assert(uploaded==visible_uploads+1 && visible.cached()==64);
    visible.clear();visible.update();
    // Failure state belongs to the requested avatar, never another worker.
    plex::Artwork retry([&](const plex::Request &){return plex::Reply{503,"",""};});
    assert(retry.avatar_state("/users/retry")=="Waiting for avatar…");
    retry.avatar("/users/retry","fixture");settle(retry);
    assert(retry.avatar_state("/users/retry").find("503")!=std::string::npos);
    retry.retry_avatars();assert(retry.avatar_state("/users/retry")=="Waiting for avatar…");
    // Active avatars survive ordinary poster churn beyond the texture-count cap.
    plex::Artwork pinned(network);pinned.avatar("/users/pinned","fixture");settle(pinned);
    auto pinned_texture=pinned.avatar("/users/pinned","fixture");assert(pinned_texture);
    for(int i=0;i<70;++i){pinned.get("https://fixture.invalid","fixture","/churn/"+std::to_string(i));settle(pinned);}
    assert(pinned.avatar("/users/pinned","fixture")==pinned_texture);
    pinned.clear();pinned.update();
    // A slow avatar must not serialize or starve independently visible media.
    std::promise<void> release_avatar;auto permit=release_avatar.get_future().share();
    plex::Artwork independent([&](const plex::Request &q){if(q.url.find("/users/slow")!=std::string::npos)permit.wait();return plex::Reply{200,image,""};});
    independent.avatar("/users/slow","account");independent.get("https://fixture.invalid","profile","/ready");
    bool media_ready=false;for(int n=0;n<500;++n){independent.update();if(independent.get("https://fixture.invalid","profile","/ready")){media_ready=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    release_avatar.set_value();settle(independent);assert(media_ready);independent.clear();independent.update();
    // A stalled backdrop also cannot block a second media image. Duplicate requests coalesce.
    std::promise<void> release_backdrop;auto backdrop_permit=release_backdrop.get_future().share();std::atomic<int> backdrop_calls{0};
    plex::Artwork media_lanes([&](const plex::Request &q){if(q.url.find("url=%2Fslow-backdrop")!=std::string::npos){++backdrop_calls;backdrop_permit.wait();}return plex::Reply{200,image,""};});
    media_lanes.landscape("https://fixture.invalid","profile","/slow-backdrop");media_lanes.landscape("https://fixture.invalid","profile","/slow-backdrop");
    media_lanes.get("https://fixture.invalid","profile","/fast-poster");bool poster_ready=false;
    for(int n=0;n<500;++n){media_lanes.update();if(media_lanes.get("https://fixture.invalid","profile","/fast-poster")){poster_ready=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    release_backdrop.set_value();settle(media_lanes);assert(poster_ready && backdrop_calls==1);media_lanes.clear();media_lanes.update();
    int missing=0,series=0;
    plex::Artwork fallback([&](const plex::Request &q){if(q.url.find("url=%2Fmissing")!=std::string::npos){++missing;return plex::Reply{404,"",""};}++series;return plex::Reply{200,image,""};});
    plex::Item episode;episode.type="episode";episode.thumb="/missing";episode.grandparent_thumb="/series";
    fallback.media("https://fixture.invalid","profile",episode,true);settle(fallback);
    fallback.media("https://fixture.invalid","profile",episode,true);settle(fallback);
    auto retained=fallback.media("https://fixture.invalid","profile",episode,true);assert(retained && missing==1 && series==1);
    for(int i=0;i<10;++i)assert(fallback.media("https://fixture.invalid","profile",episode,true)==retained);
    assert(missing==1 && series==1);fallback.clear();fallback.update();
    int attempts=0;plex::Artwork bounded_retry([&](const plex::Request &){++attempts;return plex::Reply{0,"","timeout"};});
    for(int attempt=0;attempt<6;++attempt){bounded_retry.get("https://fixture.invalid","profile","/timeout");settle(bounded_retry);for(int frame=0;frame<1000;++frame)bounded_retry.update();}
    assert(attempts==3);bounded_retry.clear();bounded_retry.update();
    plex::Artwork cloud([&](const plex::Request &q){assert(q.url=="https://metadata-static.plex.tv/poster?width=320" && q.headers.count("X-Plex-Token")==0);return plex::Reply{200,image,""};});
    cloud.get("https://fixture.invalid","profile","https://metadata-static.plex.tv/poster?width=320&X-Plex-Token=secret");settle(cloud);
    assert(cloud.get("https://fixture.invalid","profile","https://metadata-static.plex.tv/poster?width=320"));cloud.clear();cloud.update();
    // Direct authorized CDN originals may be larger than PMS-sized responses; retain bounded RGBA.
    std::string original_image(18,'\0');original_image[2]=2;original_image[12]=static_cast<char>(641&255);original_image[13]=static_cast<char>(641>>8);original_image[14]=1;original_image[16]=24;original_image.append(641*3,'\x7f');
    plex::Artwork original_cdn([&](const plex::Request &q){assert(q.headers.count("X-Plex-Token")==0);return plex::Reply{200,original_image,""};});
    original_cdn.get("https://fixture.invalid","profile","https://metadata-static.plex.tv/original");settle(original_cdn);assert(original_cdn.get("https://fixture.invalid","profile","https://metadata-static.plex.tv/original") && last_width==640 && last_height==1);original_cdn.clear();original_cdn.update();
    std::filesystem::remove_all(temporary);
    puts("Artwork regression passed: real decode, malformed input, cancellation, bounded cache, texture cleanup");
}
