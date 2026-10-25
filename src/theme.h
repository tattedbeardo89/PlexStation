// SPDX-License-Identifier: GPL-3.0-or-later
// PlexStation's native TV theme and original angular branding.
#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
namespace Theme {
inline constexpr ImU32 Background=IM_COL32(17,24,39,255), Sidebar=IM_COL32(31,41,55,255);
inline constexpr ImU32 Surface=IM_COL32(55,65,81,255), Text=IM_COL32(249,250,251,255);
inline constexpr ImU32 Secondary=IM_COL32(156,163,175,255), Accent=IM_COL32(99,102,241,255);
inline constexpr ImU32 Focus=IM_COL32(129,140,248,255), Blue=IM_COL32(59,130,246,255);
inline constexpr ImU32 Success=IM_COL32(16,185,129,255), Warning=IM_COL32(245,158,11,255);
inline constexpr ImU32 Error=IM_COL32(239,68,68,255), Border=IM_COL32(75,85,99,255);
inline constexpr ImU32 White=IM_COL32(255,255,255,255), Black=IM_COL32(0,0,0,255);
constexpr ImU32 alpha(ImU32 color,int opacity) {return (color & ~IM_COL32_A_MASK) | (static_cast<ImU32>(opacity)<<IM_COL32_A_SHIFT);}
inline constexpr ImU32 Selection=IM_COL32(55,61,103,255);
namespace Font {inline constexpr float Hero=52,Title=46,Brand=38,Heading=34,Body=28,Caption=22,Key=42,Avatar=70,Activation=58,LargeActivation=72,DeviceCode=110;}
namespace Metric {inline constexpr float Radius=14,ImageRadius=10,FocusWidth=2,Gap=16,RowHeight=70,RowStep=90,CompactStep=85,NavStep=78,CarouselStep=435,CardStep=280,SidebarWidth=330;}
inline bool reduced_motion=false;
inline float motion(float dt) {return reduced_motion?1.f:std::clamp(dt*14.f,0.f,1.f);}
inline ImU32 focus_color(double seconds) {return reduced_motion?Focus:alpha(Focus,220+static_cast<int>(35*(.5+.5*std::sin(seconds*4))));}
// Same geometry/colors as assets/plexstation.svg; no texture/network dependency.
inline void brand(ImDrawList *draw,float x,float y,float size) {
    auto polygon=[&](std::initializer_list<ImVec2> vertices,ImU32 color){
        ImVec2 points[12];int count=0;
        for(const auto &point:vertices)points[count++]=ImVec2(x+point.x*size/512,y+point.y*size/512);
        draw->AddConcavePolyFilled(points,count,color);
    };
    polygon({{104,88},{304,88},{408,192},{408,264},{304,368},{224,368},{224,440},{104,440}},Sidebar);
    polygon({{88,72},{288,72},{392,176},{392,248},{288,352},{208,352},{208,424},{88,424}},Accent);
    polygon({{88,72},{208,144},{208,424},{88,424}},Focus);
    polygon({{208,144},{316,212},{208,280}},Text);
    polygon({{288,72},{392,176},{316,212},{208,144}},Blue);
}
// Native equivalent of assets/avatar-placeholder.svg; no decoding or network dependency.
inline void profile_placeholder(ImDrawList *draw,float x,float y,float size,bool neutral){
    draw->AddCircleFilled({x+size*.5f,y+size*.5f},size*.5f,Background,32);
    if(neutral){draw->AddCircleFilled({x+size*.5f,y+size*.35f},size*.175f,Focus,24);draw->AddRectFilled({x+size*.15f,y+size*.575f},{x+size*.85f,y+size},Focus,size*.2125f);}
}
inline void cover_uv(float source_aspect,float width,float height,ImVec2 &uv0,ImVec2 &uv1) {
    uv0=ImVec2(0,0);uv1=ImVec2(1,1);
    if(source_aspect<=0 || width<=0 || height<=0)return;
    if(width/height>source_aspect) {
        float visible=height*source_aspect/width;
        uv0.y=(1-visible)*.5f;uv1.y=1-uv0.y;
    } else {
        float visible=width/(height*source_aspect);
        uv0.x=(1-visible)*.5f;uv1.x=1-uv0.x;
    }
}
inline void apply(float scale=1.f) {
    auto &s=ImGui::GetStyle();
    s.WindowRounding=s.ChildRounding=s.PopupRounding=Metric::Radius*scale;
    s.FrameRounding=s.GrabRounding=Metric::ImageRadius*scale;
    s.WindowPadding=ImVec2(24*scale,24*scale);s.FramePadding=ImVec2(16*scale,12*scale);
    s.ItemSpacing=ImVec2(Metric::Gap*scale,Metric::Gap*scale);
    auto set=[&](ImGuiCol index,ImU32 color){s.Colors[index]=ImGui::ColorConvertU32ToFloat4(color);};
    set(ImGuiCol_Text,Text);set(ImGuiCol_TextDisabled,Secondary);
    set(ImGuiCol_WindowBg,Background);set(ImGuiCol_ChildBg,Sidebar);set(ImGuiCol_PopupBg,Sidebar);
    set(ImGuiCol_Border,Border);set(ImGuiCol_FrameBg,Surface);set(ImGuiCol_FrameBgHovered,Selection);set(ImGuiCol_FrameBgActive,Selection);
    set(ImGuiCol_Button,Surface);set(ImGuiCol_ButtonHovered,Accent);set(ImGuiCol_ButtonActive,Focus);
    set(ImGuiCol_Header,Selection);set(ImGuiCol_HeaderHovered,Accent);set(ImGuiCol_HeaderActive,Focus);
    set(ImGuiCol_CheckMark,Focus);set(ImGuiCol_SliderGrab,Accent);set(ImGuiCol_SliderGrabActive,Focus);
    set(ImGuiCol_NavCursor,Focus);set(ImGuiCol_Separator,Border);set(ImGuiCol_TextSelectedBg,Selection);
    set(ImGuiCol_TitleBg,Sidebar);set(ImGuiCol_TitleBgActive,Selection);
    set(ImGuiCol_Tab,Sidebar);set(ImGuiCol_TabSelected,Accent);set(ImGuiCol_TabHovered,Focus);
}
}
