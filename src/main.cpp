#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <cstring>
#include <cstdio>
#include <string>

#include "font.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Palette
// ─────────────────────────────────────────────────────────────────────────────
namespace c {
    constexpr ImVec4 accent     = {0.50f, 0.35f, 0.88f, 1.f};
    constexpr ImVec4 accent_dim = {0.32f, 0.22f, 0.60f, 1.f};
    constexpr ImVec4 accent_glow= {0.50f, 0.35f, 0.88f, 0.18f};
    constexpr ImVec4 bg         = {0.036f,0.036f,0.048f,1.f};
    constexpr ImVec4 bg_panel   = {0.052f,0.050f,0.068f,1.f};
    constexpr ImVec4 bg_widget  = {0.076f,0.072f,0.100f,1.f};
    constexpr ImVec4 stroke     = {0.110f,0.100f,0.155f,1.f};
    constexpr ImVec4 text       = {0.880f,0.860f,0.960f,1.f};
    constexpr ImVec4 text_dim   = {0.340f,0.320f,0.430f,1.f};
    constexpr ImVec4 green      = {0.28f, 0.76f, 0.46f, 1.f};
    constexpr ImVec4 red        = {0.76f, 0.26f, 0.26f, 1.f};
    constexpr ImVec4 yellow     = {0.88f, 0.72f, 0.18f, 1.f};
}
static ImU32 C(ImVec4 v, float a=1.f){
    return IM_COL32(int(v.x*255),int(v.y*255),int(v.z*255),int(a*255));
}

namespace font {
    inline ImFont* bold    = nullptr;
    inline ImFont* regular = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  X11 / click
// ─────────────────────────────────────────────────────────────────────────────
static Display* g_dpy  = nullptr;
static Window   g_win  = 0;
static auto     g_winT = std::chrono::steady_clock::now();

static void updateFocus() {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now-g_winT).count() < 500) return;
    g_winT = now;
    Window focused; int revert;
    XGetInputFocus(g_dpy, &focused, &revert);
    if (focused <= 1) return;
    char* name = nullptr;
    if (XFetchName(g_dpy, focused, &name) && name) {
        g_win = (strstr(name,"Minecraft")||strstr(name,"minecraft")) ? focused : 0;
        XFree(name);
    }
}

static void injectClick(int button) {
    if (!g_dpy || !g_win) return;
    Window root,child; int rx,ry,wx,wy; unsigned int mask;
    XQueryPointer(g_dpy,g_win,&root,&child,&rx,&ry,&wx,&wy,&mask);
    XEvent ev; memset(&ev,0,sizeof(ev));
    ev.xbutton.button=button; ev.xbutton.same_screen=True; ev.xbutton.subwindow=g_win;
    while(ev.xbutton.subwindow){
        ev.xbutton.window=ev.xbutton.subwindow;
        XQueryPointer(g_dpy,ev.xbutton.window,&root,&ev.xbutton.subwindow,
                      &ev.xbutton.x_root,&ev.xbutton.y_root,
                      &ev.xbutton.x,&ev.xbutton.y,&ev.xbutton.state);
    }
    ev.type=ButtonPress;
    XSendEvent(g_dpy,ev.xbutton.window,True,ButtonPressMask,&ev); XFlush(g_dpy);
    ev.type=ButtonRelease;
    XSendEvent(g_dpy,ev.xbutton.window,True,ButtonReleaseMask,&ev); XFlush(g_dpy);
}

static bool isLMBHeld(){
    if(!g_dpy) return false;
    Window r,ch; int rx,ry,wx,wy; unsigned int mask;
    XQueryPointer(g_dpy,DefaultRootWindow(g_dpy),&r,&ch,&rx,&ry,&wx,&wy,&mask);
    return mask & Button1Mask;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bind
// ─────────────────────────────────────────────────────────────────────────────
struct Bind {
    SDL_Keycode key    = SDLK_UNKNOWN;
    bool        waiting= false;
    std::string name() const {
        if(waiting)            return "...";
        if(key==SDLK_UNKNOWN)  return "None";
        return SDL_GetKeyName(key);
    }
    void onKey(SDL_Keycode k){
        if(!waiting) return;
        // re-grab com nova tecla
        if(g_dpy) XUngrabKey(g_dpy, AnyKey, AnyModifier, DefaultRootWindow(g_dpy));
        if(k==SDLK_ESCAPE){ key=SDLK_UNKNOWN; waiting=false; return; }
        key=k; waiting=false;
        // converte SDL keycode → X11 KeySym → KeyCode e faz grab global
        KeySym ks = XStringToKeysym(SDL_GetKeyName(k));
        if(ks==NoSymbol) ks=(KeySym)k; // fallback direto
        KeyCode kc=XKeysymToKeycode(g_dpy,ks);
        if(kc) XGrabKey(g_dpy,kc,AnyModifier,DefaultRootWindow(g_dpy),False,GrabModeAsync,GrabModeAsync);
    }
} g_bind;

// ─────────────────────────────────────────────────────────────────────────────
//  Clicker
// ─────────────────────────────────────────────────────────────────────────────
struct Clicker {
    std::atomic<bool>  enabled {false};
    std::atomic<float> minCPS  {10.f};
    std::atomic<float> maxCPS  {14.f};
    std::atomic<bool>  holdOnly{true};
    std::atomic<bool>  running {true};
    std::thread        worker;
    void start(){ worker=std::thread([this]{loop();}); }
    void stop() { running=false; if(worker.joinable()) worker.join(); }
private:
    void loop(){
        std::mt19937 rng{std::random_device{}()};
        auto last=std::chrono::steady_clock::now();
        long long delay=80;
        while(running){
            updateFocus();
            if(enabled && g_win && (!holdOnly||isLMBHeld())){
                auto now=std::chrono::steady_clock::now();
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count()>=delay){
                    injectClick(1);
                    last=now;
                    float lo=minCPS.load(),hi=maxCPS.load();
                    if(lo>hi) lo=hi;
                    delay=(long long)(1000.f/std::uniform_real_distribution<float>(lo,hi)(rng));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
} g_clicker;

// ─────────────────────────────────────────────────────────────────────────────
//  Style
// ─────────────────────────────────────────────────────────────────────────────
static void ApplyStyle(){
    ImGuiStyle& s=ImGui::GetStyle();
    s.WindowRounding=12.f; s.ChildRounding=8.f; s.FrameRounding=6.f;
    s.GrabRounding=6.f;    s.PopupRounding=8.f; s.ScrollbarRounding=6.f;
    s.WindowBorderSize=1.f; s.FrameBorderSize=0.f;
    s.ItemSpacing={8.f,8.f}; s.FramePadding={10.f,6.f}; s.WindowPadding={18.f,18.f};
    auto* col=s.Colors;
    col[ImGuiCol_WindowBg]             =c::bg;
    col[ImGuiCol_ChildBg]              =c::bg_panel;
    col[ImGuiCol_Border]               =c::stroke;
    col[ImGuiCol_FrameBg]              =c::bg_widget;
    col[ImGuiCol_FrameBgHovered]       ={0.11f,0.10f,0.15f,1.f};
    col[ImGuiCol_FrameBgActive]        ={0.15f,0.13f,0.20f,1.f};
    col[ImGuiCol_SliderGrab]           =c::accent;
    col[ImGuiCol_SliderGrabActive]     =c::accent_dim;
    col[ImGuiCol_CheckMark]            =c::accent;
    col[ImGuiCol_Button]               =c::bg_widget;
    col[ImGuiCol_ButtonHovered]        ={0.13f,0.11f,0.19f,1.f};
    col[ImGuiCol_ButtonActive]         =c::accent_dim;
    col[ImGuiCol_Text]                 =c::text;
    col[ImGuiCol_TextDisabled]         =c::text_dim;
    col[ImGuiCol_TitleBg]              =c::bg;
    col[ImGuiCol_TitleBgActive]        =c::bg;
    col[ImGuiCol_ScrollbarBg]          =c::bg_panel;
    col[ImGuiCol_ScrollbarGrab]        =c::stroke;
    col[ImGuiCol_ScrollbarGrabHovered] =c::accent_dim;
    col[ImGuiCol_ScrollbarGrabActive]  =c::accent;
    col[ImGuiCol_PopupBg]              =c::bg_panel;
    col[ImGuiCol_Separator]            =c::stroke;
    col[ImGuiCol_Header]               =c::bg_widget;
    col[ImGuiCol_HeaderHovered]        ={0.13f,0.11f,0.19f,1.f};
    col[ImGuiCol_HeaderActive]         =c::accent_dim;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Widgets
// ─────────────────────────────────────────────────────────────────────────────
static void Toggle(const char* id, std::atomic<bool>& val){
    bool v=val.load();
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    const float W=40.f,H=22.f,R=H*.5f;
    ImGui::InvisibleButton(id,{W,H});
    if(ImGui::IsItemClicked()){v=!v;val.store(v);}
    bool hov=ImGui::IsItemHovered();
    // glow quando ativo
    if(v) dl->AddRectFilled({p.x-2.f,p.y-2.f},{p.x+W+2.f,p.y+H+2.f},C(c::accent,.12f),R+2.f);
    ImU32 bg =v?C(c::accent,hov?.88f:1.f):C(c::bg_widget);
    ImU32 brd=v?C(c::accent,.5f):C(c::stroke);
    dl->AddRectFilled(p,{p.x+W,p.y+H},bg,R);
    dl->AddRect(p,{p.x+W,p.y+H},brd,R,0,1.f);
    float cx=p.x+R+(v?W-H:0.f);
    // knob com sombra
    dl->AddCircleFilled({cx+1.f,p.y+R+1.f},R-3.f,IM_COL32(0,0,0,60));
    dl->AddCircleFilled({cx,p.y+R},R-3.f,IM_COL32(255,255,255,240));
}

// Row: label à esquerda, widget à direita, dentro de um card
static void RowLabel(const char* title, const char* sub=nullptr){
    ImGui::PushFont(font::bold);
    ImGui::PushStyleColor(ImGuiCol_Text,c::text);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    if(sub){
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim);
        ImGui::TextUnformatted(sub);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
}

// Divider com label
static void Divider(const char* label){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    float w=ImGui::GetContentRegionAvail().x;
    ImGui::PushFont(font::regular);
    ImVec2 ts=ImGui::CalcTextSize(label);
    ImGui::PopFont();
    float lx=p.x, rx=p.x+w;
    float cy=p.y+ts.y*.5f;
    dl->AddLine({lx,cy},{lx+10.f,cy},C(c::stroke,0.8f),1.f);
    dl->AddText(font::regular,12.f,{lx+14.f,p.y},C(c::text_dim,0.7f),label);
    dl->AddLine({lx+18.f+ts.x,cy},{rx,cy},C(c::stroke,0.8f),1.f);
    ImGui::Dummy({w,ts.y+6.f});
}

// Card com borda e accent top
struct Card {
    ImVec2 o; float w,h;
    Card(float height): h(height){
        o=ImGui::GetCursorScreenPos();
        w=ImGui::GetContentRegionAvail().x;
        ImDrawList* dl=ImGui::GetWindowDrawList();
        // sombra sutil
        dl->AddRectFilled({o.x+2.f,o.y+2.f},{o.x+w+2.f,o.y+h+2.f},IM_COL32(0,0,0,40),8.f);
        dl->AddRectFilled(o,{o.x+w,o.y+h},C(c::bg_panel),8.f);
        dl->AddRect(o,{o.x+w,o.y+h},C(c::stroke),8.f,0,1.f);
        // accent line topo
        dl->AddRectFilled(o,{o.x+w,o.y+2.f},C(c::accent,.65f),8.f);
        ImGui::SetCursorScreenPos({o.x+14.f,o.y+10.f});
        ImGui::PushClipRect(o,{o.x+w,o.y+h},true);
    }
    ~Card(){
        ImGui::PopClipRect();
        ImGui::SetCursorScreenPos({o.x,o.y+h+10.f});
        ImGui::Dummy({0.f,0.f});
    }
};

// Slider estilizado com label inline
static void LabeledSlider(const char* id, const char* label, float* v, float lo, float hi, ImVec2 origin, float width){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImGui::PushFont(font::regular);
    ImVec2 ts=ImGui::CalcTextSize(label);
    dl->AddText({origin.x,origin.y},C(c::text_dim),label);
    // valor
    char vbuf[16]; snprintf(vbuf,sizeof(vbuf),"%.1f",*v);
    ImVec2 vs=ImGui::CalcTextSize(vbuf);
    dl->AddText({origin.x+width-vs.x,origin.y},C(c::accent),vbuf);
    ImGui::PopFont();

    ImGui::SetCursorScreenPos({origin.x,origin.y+ts.y+4.f});
    ImGui::PushItemWidth(width);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,c::accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,c::accent_dim);
    ImGui::SliderFloat(id,v,lo,hi,"");
    ImGui::PopStyleColor(2);
    ImGui::PopItemWidth();
}

// ─────────────────────────────────────────────────────────────────────────────
//  UI
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  UI  — 360×450, keybind no topo
// ─────────────────────────────────────────────────────────────────────────────
static void RenderUI(){
    constexpr float W=360.f, H=450.f;
    ImGuiIO& io=ImGui::GetIO();
    ImGui::SetNextWindowPos({(io.DisplaySize.x-W)*.5f,(io.DisplaySize.y-H)*.5f},ImGuiCond_Always);
    ImGui::SetNextWindowSize({W,H},ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.f);
    ImGui::Begin("##oxy",nullptr,
        ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 wp=ImGui::GetWindowPos();
    float cw=W-36.f;

    // ── Header ────────────────────────────────────────────────────────────────
    {
        dl->AddRectFilled(wp,{wp.x+W,wp.y+58.f},C(c::bg_panel),12.f);
        dl->AddRectFilled({wp.x,wp.y+46.f},{wp.x+W,wp.y+58.f},C(c::bg_panel));
        dl->AddLine({wp.x,wp.y+58.f},{wp.x+W,wp.y+58.f},C(c::stroke));
        dl->AddRectFilled(wp,{wp.x+W,wp.y+3.f},C(c::accent));

        dl->AddCircleFilled({wp.x+26.f,wp.y+29.f},14.f,C(c::accent,.08f));
        dl->AddCircle({wp.x+26.f,wp.y+29.f},11.f,C(c::accent,.35f),32,1.5f);
        dl->AddCircle({wp.x+26.f,wp.y+29.f}, 8.f,C(c::accent,.70f),32,1.5f);

        ImGui::SetCursorPos({46.f,12.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text);   ImGui::Text("Oxygen"); ImGui::PopStyleColor();
        ImGui::SameLine(0,0);
        ImGui::PushStyleColor(ImGuiCol_Text,c::accent); ImGui::Text("Clicker"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorPos({46.f,32.f});
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("v1.0  ·  Linux AutoClicker"); ImGui::PopStyleColor();
        ImGui::PopFont();

        // badge
        bool on=g_clicker.enabled.load(), focused=(g_win!=0);
        ImVec4 bc; const char* bt;
        if(on&&focused){bc=c::green;bt="ACTIVE";}
        else if(on&&!focused){bc=c::yellow;bt="NO FOCUS";}
        else{bc=c::red;bt="IDLE";}
        ImGui::PushFont(font::regular);
        ImVec2 ts=ImGui::CalcTextSize(bt);
        ImGui::PopFont();
        float bx=wp.x+W-ts.x-26.f, by=wp.y+19.f;
        dl->AddRectFilled({bx-8.f,by-5.f},{bx+ts.x+8.f,by+ts.y+5.f},C(bc,.12f),6.f);
        dl->AddRect({bx-8.f,by-5.f},{bx+ts.x+8.f,by+ts.y+5.f},C(bc,.50f),6.f,0,1.f);
        dl->AddCircleFilled({bx-2.f,by+ts.y*.5f},3.f,C(bc));
        dl->AddText(font::regular,12.f,{bx+4.f,by},C(bc),bt);
    }

    ImGui::SetCursorPos({18.f,66.f});
    ImGui::BeginGroup();

    // ── Keybind ───────────────────────────────────────────────────────────────
    Divider("KEYBIND");
    {
        Card card(62.f);
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+10.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text); ImGui::Text("Toggle key"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+32.f});
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,g_bind.waiting?c::yellow:c::text_dim);
        ImGui::Text(g_bind.waiting?"Press any key  (Esc = clear)":"Global hotkey — works in any window");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        std::string bn=g_bind.name();
        ImVec4 bc=g_bind.waiting?c::yellow:c::accent;
        ImGui::SetCursorScreenPos({card.o.x+card.w-88.f,card.o.y+17.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Button,       {bc.x,bc.y,bc.z,.15f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{bc.x,bc.y,bc.z,.28f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {bc.x,bc.y,bc.z,.42f});
        ImGui::PushStyleColor(ImGuiCol_Text,bc);
        ImGui::PushStyleColor(ImGuiCol_Border,{bc.x,bc.y,bc.z,.45f});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,6.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        if(ImGui::Button(bn.c_str(),{74.f,28.f}))
            if(!g_bind.waiting) g_bind.waiting=true;
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(5); ImGui::PopFont();
    }

    // ── Enable ────────────────────────────────────────────────────────────────
    Divider("AUTOCLICKER");
    {
        Card card(58.f);
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+10.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text); ImGui::Text("Enable"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+30.f});
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Toggle clicking on/off"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorScreenPos({card.o.x+card.w-54.f,card.o.y+18.f});
        Toggle("##en",g_clicker.enabled);
    }

    // ── CPS ───────────────────────────────────────────────────────────────────
    Divider("CLICKS PER SECOND");
    {
        Card card(116.f);
        float lo=g_clicker.minCPS.load(), hi=g_clicker.maxCPS.load();
        LabeledSlider("##lo","Min CPS",&lo,1.f,20.f,{card.o.x+14.f,card.o.y+8.f},cw-28.f);
        if(lo!=g_clicker.minCPS.load()) g_clicker.minCPS.store(lo);
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+60.f});
        LabeledSlider("##hi","Max CPS",&hi,1.f,20.f,{card.o.x+14.f,card.o.y+60.f},cw-28.f);
        if(hi!=g_clicker.maxCPS.load()) g_clicker.maxCPS.store(hi);
        if(g_clicker.minCPS.load()>g_clicker.maxCPS.load())
            g_clicker.minCPS.store(g_clicker.maxCPS.load());
    }

    // ── Options ───────────────────────────────────────────────────────────────
    Divider("OPTIONS");
    {
        Card card(58.f);
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+10.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text); ImGui::Text("Hold to click"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorScreenPos({card.o.x+14.f,card.o.y+30.f});
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Only click while LMB is held"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorScreenPos({card.o.x+card.w-54.f,card.o.y+18.f});
        Toggle("##hold",g_clicker.holdOnly);
    }

    ImGui::EndGroup();

    // ── Footer ────────────────────────────────────────────────────────────────
    {
        float fy=wp.y+H-24.f;
        dl->AddLine({wp.x+18.f,fy},{wp.x+W-18.f,fy},C(c::stroke,.4f));
        dl->AddText(font::regular,11.f,{wp.x+18.f,fy+7.f},C(c::text_dim,.28f),
                    "OxygenClicker  ·  github.com/revann/OxygenClicker");
    }
    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int,char**){
    XInitThreads();
    g_dpy=XOpenDisplay(nullptr);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);

    SDL_Window* win=SDL_CreateWindow("OxygenClicker",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,360,450,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO();
    io.IniFilename=nullptr;

    auto copyFont=[](const unsigned char* src,size_t sz)->void*{
        void* d=IM_ALLOC(sz); memcpy(d,src,sz); return d;
    };
    ImFontConfig cfg; cfg.FontDataOwnedByAtlas=true;
    const ImWchar* ranges=io.Fonts->GetGlyphRangesDefault();
    font::bold   =io.Fonts->AddFontFromMemoryTTF(copyFont(lexend_bold,   sizeof(lexend_bold)),   sizeof(lexend_bold),   15.f,&cfg,ranges);
    font::regular=io.Fonts->AddFontFromMemoryTTF(copyFont(lexend_regular,sizeof(lexend_regular)),sizeof(lexend_regular),13.f,&cfg,ranges);
    io.Fonts->Build();

    ApplyStyle();
    ImGui_ImplSDL2_InitForOpenGL(win,ctx);
    ImGui_ImplOpenGL3_Init("#version 130");

    g_clicker.start();

    bool quit=false;
    while(!quit){
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            ImGui_ImplSDL2_ProcessEvent(&e);
            if(e.type==SDL_QUIT) quit=true;
            if(e.type==SDL_KEYDOWN && g_bind.waiting)
                g_bind.onKey(e.key.keysym.sym);
        }
        // Bind global via XGrabKey — recebe KeyPress mesmo sem foco
        if(g_dpy && !g_bind.waiting && g_bind.key!=SDLK_UNKNOWN){
            while(XPending(g_dpy)){
                XEvent xe; XNextEvent(g_dpy,&xe);
                if(xe.type==KeyPress)
                    g_clicker.enabled.store(!g_clicker.enabled.load());
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        RenderUI();
        ImGui::Render();
        glViewport(0,0,(int)io.DisplaySize.x,(int)io.DisplaySize.y);
        glClearColor(c::bg.x,c::bg.y,c::bg.z,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    if(g_dpy) XUngrabKey(g_dpy,AnyKey,AnyModifier,DefaultRootWindow(g_dpy));
    g_clicker.stop();
    if(g_dpy) XCloseDisplay(g_dpy);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
