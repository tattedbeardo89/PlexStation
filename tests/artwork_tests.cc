// Production decoding/cache lifecycle with fake transport and GPU upload only.
#include "plex/artwork.h"
#include "library.h"
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#include "stb_image.h"
#pragma GCC diagnostic pop
#include <cassert>
#include <chrono>
#include <thread>
static int uploaded,freed;
ImTextureID library_upload_rgba(const unsigned char *rgba,int w,int h) {
    assert(rgba && w==1 && h==1); return ++uploaded;
}
void library_free_image(ImTextureID texture) {if(texture) ++freed;}
static void settle(plex::Artwork &art) {
    for(int i=0;i<100;++i) {std::this_thread::sleep_for(std::chrono::milliseconds(2)); art.update();}
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
    assert(uploaded==15 && freed==3); art.clear(); assert(freed==uploaded);
    puts("Artwork regression passed: real decode, malformed input, cancellation, bounded cache, texture cleanup");
}
