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
static int uploaded,freed;
ImTextureID library_upload_rgba(const unsigned char *rgba,int w,int h) {
    assert(rgba && w>0 && h>0 && w<=256 && h<=256); return ++uploaded;
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
    assert(uploaded==15 && freed==3); art.clear();assert(freed==3);
    art.update();assert(freed==uploaded); // current-frame references survive until the next frame
    plex::diagnostic_flags=plex::NoDownloads; assert(!art.get("https://fixture.invalid","fixture","/no-download")); settle(art); assert(uploaded==15);
    plex::diagnostic_flags=plex::NoDecode; art.get("https://fixture.invalid","fixture","/no-decode");settle(art);assert(uploaded==15);
    plex::diagnostic_flags=plex::NoUpload; art.get("https://fixture.invalid","fixture","/no-upload");settle(art);assert(uploaded==15);
    plex::diagnostic_flags=0; art.clear();
    assert(!art.avatar("https://evil.invalid/avatar","fixture"));
    assert(!art.avatar("http://plex.tv/avatar","fixture"));
    art.avatar("https://plex.tv/users/fixture/avatar","fixture");settle(art);assert(uploaded==16);auto before_clear=freed;art.clear();assert(freed==before_clear);art.update();assert(freed==uploaded);
    int requests=0;
    plex::Artwork redirected([&](const plex::Request &q){
        ++requests;
        if(q.url=="https://plex.tv/users/avatar") {
            assert(q.headers.at("X-Plex-Token")=="private-fixture");
            plex::Reply reply{302,"",""};reply.location="https://images.plex.tv/avatar?X-Plex-Token=private-fixture";return reply;
        }
        assert(q.url=="https://images.plex.tv/avatar" && q.headers.empty());
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
    assert(large.avatar("https://images.plex.tv/large","fixture"));large.clear();large.update();
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
    puts("Artwork regression passed: real decode, malformed input, cancellation, bounded cache, texture cleanup");
}
