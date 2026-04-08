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

namespace c {
    constexpr ImVec4 accent    = {0.50f,0.35f,0.88f,1.f};
    constexpr ImVec4 accent_dim= {0.32f,0.22f,0.60f,1.f};
    constexpr ImVec4 bg        = {0.036f,0.036f,0.048f,1.f};
    constexpr ImVec4 bg_panel  = {0.052f,0.050f,0.068f,1.f};
    constexpr ImVec4 bg_widget = {0.076f,0.072f,0.100f,1.f};
    constexpr ImVec4 stroke    = {0.110f,0.100f,0.155f,1.f};
    constexpr ImVec4 text      = {0.880f,0.860f,0.960f,1.f};
    constexpr ImVec4 text_dim  = {0.340f,0.320f,0.430f,1.f};
    constexpr ImVec4 green     = {0.28f,0.76f,0.46f,1.f};
    constexpr ImVec4 red       = {0.76f,0.26f,0.26f,1.f};
    constexpr ImVec4 yellow    = {0.88f,0.72f,0.18f,1.f};
}
static ImU32 C(ImVec4 v,float a=1.f){
    return IM_COL32(int(v.x*255),int(v.y*255),int(v.z*255),int(a*255));
}
namespace font { inline ImFont* bold=nullptr; inline ImFont* regular=nullptr; }

// ── X11: dois displays separados — sem race condition ─────────────────────────
// g_dpy_ui  → thread principal (XGrabKey / XPending)
// g_dpy_clk → thread do clicker (XSendEvent / XQueryPointer)
static Display*              g_dpy_ui  = nullptr;
static Display*              g_dpy_clk = nullptr;
static std::atomic<Window>   g_win     {0};
static auto                  g_winT    = std::chrono::steady_clock::now();

static void updateFocus(){
    auto now=std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::milliseconds>(now-g_winT).count()<500) return;
    g_winT=now;
    Window focused; int revert;
    XGetInputFocus(g_dpy_clk,&focused,&revert);
    if(focused<=1){g_win=0;return;}
    char* name=nullptr;
    if(XFetchName(g_dpy_clk,focused,&name)&&name){
        g_win=(strstr(name,"Minecraft")||strstr(name,"minecraft"))?focused:0;
        XFree(name);
    } else g_win=0;
}

static void injectClick(){
    if(!g_dpy_clk||!g_win) return;
    Window root,child; int rx,ry,wx,wy; unsigned int mask;
    XQueryPointer(g_dpy_clk,g_win,&root,&child,&rx,&ry,&wx,&wy,&mask);
    XEvent ev; memset(&ev,0,sizeof(ev));
    ev.xbutton.button=1; ev.xbutton.same_screen=True; ev.xbutton.subwindow=g_win;
    while(ev.xbutton.subwindow){
        ev.xbutton.window=ev.xbutton.subwindow;
        XQueryPointer(g_dpy_clk,ev.xbutton.window,&root,&ev.xbutton.subwindow,
                      &ev.xbutton.x_root,&ev.xbutton.y_root,
                      &ev.xbutton.x,&ev.xbutton.y,&ev.xbutton.state);
    }
    ev.type=ButtonPress;
    XSendEvent(g_dpy_clk,ev.xbutton.window,True,ButtonPressMask,&ev); XFlush(g_dpy_clk);
    ev.type=ButtonRelease;
    XSendEvent(g_dpy_clk,ev.xbutton.window,True,ButtonReleaseMask,&ev); XFlush(g_dpy_clk);
}

static bool isLMBHeld(){
    if(!g_dpy_clk) return false;
    Window r,ch; int rx,ry,wx,wy; unsigned int mask;
    XQueryPointer(g_dpy_clk,DefaultRootWindow(g_dpy_clk),&r,&ch,&rx,&ry,&wx,&wy,&mask);
    return mask&Button1Mask;
}

// ── Bind ──────────────────────────────────────────────────────────────────────
static KeySym sdlToKeySym(SDL_Keycode k){
    if(k>='a'&&k<='z') return (KeySym)k;
    if(k>='0'&&k<='9') return (KeySym)k;
    switch(k){
        case SDLK_F1:  return XK_F1;  case SDLK_F2:  return XK_F2;
        case SDLK_F3:  return XK_F3;  case SDLK_F4:  return XK_F4;
        case SDLK_F5:  return XK_F5;  case SDLK_F6:  return XK_F6;
        case SDLK_F7:  return XK_F7;  case SDLK_F8:  return XK_F8;
        case SDLK_F9:  return XK_F9;  case SDLK_F10: return XK_F10;
        case SDLK_F11: return XK_F11; case SDLK_F12: return XK_F12;
        case SDLK_INSERT:   return XK_Insert;
        case SDLK_DELETE:   return XK_Delete;
        case SDLK_HOME:     return XK_Home;
        case SDLK_END:      return XK_End;
        case SDLK_PAGEUP:   return XK_Prior;
        case SDLK_PAGEDOWN: return XK_Next;
        case SDLK_RETURN:   return XK_Return;
        case SDLK_SPACE:    return XK_space;
        case SDLK_TAB:      return XK_Tab;
        default:            return NoSymbol;
    }
}

struct Bind {
    SDL_Keycode key    = SDLK_UNKNOWN;
    bool        waiting= false;
    std::string name() const {
        if(waiting)           return "...";
        if(key==SDLK_UNKNOWN) return "None";
        return SDL_GetKeyName(key);
    }
    void set(SDL_Keycode k){
        if(g_dpy_ui) XUngrabKey(g_dpy_ui,AnyKey,AnyModifier,DefaultRootWindow(g_dpy_ui));
        waiting=false;
        if(k==SDLK_ESCAPE){ key=SDLK_UNKNOWN; return; }
        key=k;
        KeySym ks=sdlToKeySym(k);
        if(ks==NoSymbol) return;
        KeyCode kc=XKeysymToKeycode(g_dpy_ui,ks);
        if(kc) XGrabKey(g_dpy_ui,kc,AnyModifier,DefaultRootWindow(g_dpy_ui),
                        False,GrabModeAsync,GrabModeAsync);
    }
} g_bind;

// ── Clicker ───────────────────────────────────────────────────────────────────
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
            if(enabled&&g_win&&(!holdOnly||isLMBHeld())){
                auto now=std::chrono::steady_clock::now();
                if(std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count()>=delay){
                    injectClick();
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

// ── Style ─────────────────────────────────────────────────────────────────────
static void ApplyStyle(){
    ImGuiStyle& s=ImGui::GetStyle();
    s.WindowRounding=10.f; s.ChildRounding=7.f; s.FrameRounding=5.f;
    s.GrabRounding=5.f; s.PopupRounding=7.f;
    s.WindowBorderSize=1.f; s.FrameBorderSize=0.f;
    s.ItemSpacing={8.f,6.f}; s.FramePadding={10.f,5.f}; s.WindowPadding={16.f,16.f};
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
    col[ImGuiCol_PopupBg]              =c::bg_panel;
    col[ImGuiCol_Separator]            =c::stroke;
    col[ImGuiCol_Header]               =c::bg_widget;
    col[ImGuiCol_HeaderHovered]        ={0.13f,0.11f,0.19f,1.f};
    col[ImGuiCol_HeaderActive]         =c::accent_dim;
    col[ImGuiCol_ScrollbarBg]          =c::bg_panel;
    col[ImGuiCol_ScrollbarGrab]        =c::stroke;
    col[ImGuiCol_ScrollbarGrabHovered] =c::accent_dim;
    col[ImGuiCol_ScrollbarGrabActive]  =c::accent;
}

// ── Widgets ───────────────────────────────────────────────────────────────────
static void Toggle(const char* id, std::atomic<bool>& val){
    bool v=val.load();
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    const float W=38.f,H=20.f,R=H*.5f;
    ImGui::InvisibleButton(id,{W,H});
    if(ImGui::IsItemClicked()){v=!v;val.store(v);}
    bool hov=ImGui::IsItemHovered();
    if(v) dl->AddRectFilled({p.x-2.f,p.y-2.f},{p.x+W+2.f,p.y+H+2.f},C(c::accent,.10f),R+2.f);
    dl->AddRectFilled(p,{p.x+W,p.y+H},v?C(c::accent,hov?.85f:1.f):C(c::bg_widget),R);
    dl->AddRect(p,{p.x+W,p.y+H},v?C(c::accent,.5f):C(c::stroke),R,0,1.f);
    float cx=p.x+R+(v?W-H:0.f);
    dl->AddCircleFilled({cx+1.f,p.y+R+1.f},R-3.f,IM_COL32(0,0,0,50));
    dl->AddCircleFilled({cx,p.y+R},R-3.f,IM_COL32(255,255,255,240));
}

// Linha de row: título + subtítulo à esquerda, widget à direita
// Retorna a posição X onde o widget deve ser colocado
static void RowBegin(ImVec2 origin, float cardW, const char* title, const char* sub){
    ImGui::SetCursorScreenPos({origin.x+14.f,origin.y+10.f});
    ImGui::PushFont(font::bold);
    ImGui::PushStyleColor(ImGuiCol_Text,c::text);
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor(); ImGui::PopFont();
    if(sub){
        ImGui::SetCursorScreenPos({origin.x+14.f,origin.y+30.f});
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim);
        ImGui::TextUnformatted(sub);
        ImGui::PopStyleColor(); ImGui::PopFont();
    }
    (void)cardW;
}

static void Divider(const char* label){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    float w=ImGui::GetContentRegionAvail().x;
    ImVec2 ts=ImGui::CalcTextSize(label);
    float cy=p.y+ts.y*.5f;
    dl->AddLine({p.x,cy},{p.x+8.f,cy},C(c::stroke,.8f));
    dl->AddText(font::regular,11.f,{p.x+12.f,p.y},C(c::text_dim,.65f),label);
    dl->AddLine({p.x+16.f+ts.x,cy},{p.x+w,cy},C(c::stroke,.8f));
    ImGui::Dummy({w,ts.y+5.f});
}

// Card simples: desenha fundo+borda+accent, avança cursor
struct Card {
    ImVec2 o; float w,h;
    Card(float height):h(height){
        o=ImGui::GetCursorScreenPos();
        w=ImGui::GetContentRegionAvail().x;
        ImDrawList* dl=ImGui::GetWindowDrawList();
        dl->AddRectFilled({o.x+2.f,o.y+3.f},{o.x+w+2.f,o.y+h+3.f},IM_COL32(0,0,0,35),8.f);
        dl->AddRectFilled(o,{o.x+w,o.y+h},C(c::bg_panel),8.f);
        dl->AddRect(o,{o.x+w,o.y+h},C(c::stroke),8.f,0,1.f);
        dl->AddRectFilled(o,{o.x+w,o.y+2.f},C(c::accent,.6f),8.f);
        // reserva espaço — sem PushClipRect pra não quebrar layout
        ImGui::SetCursorScreenPos({o.x,o.y+h+8.f});
        ImGui::Dummy({0.f,0.f});
    }
};

static void SliderRow(const char* id, const char* label, float* v, float lo, float hi, float x, float y, float w){
    ImDrawList* dl=ImGui::GetWindowDrawList();
    char vbuf[16]; snprintf(vbuf,sizeof(vbuf),"%.1f",*v);
    ImVec2 vs=ImGui::CalcTextSize(vbuf);
    dl->AddText(font::regular,12.f,{x,y},C(c::text_dim),label);
    dl->AddText(font::regular,12.f,{x+w-vs.x,y},C(c::accent),vbuf);
    ImGui::SetCursorScreenPos({x,y+14.f});
    ImGui::PushItemWidth(w);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,c::accent);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,c::accent_dim);
    ImGui::SliderFloat(id,v,lo,hi,"");
    ImGui::PopStyleColor(2); ImGui::PopItemWidth();
}

// ── UI ────────────────────────────────────────────────────────────────────────
static void RenderUI(){
    constexpr float W=360.f, H=430.f;
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
    float cw=W-32.f;

    // ── Header ────────────────────────────────────────────────────────────────
    {
        dl->AddRectFilled(wp,{wp.x+W,wp.y+56.f},C(c::bg_panel),10.f);
        dl->AddRectFilled({wp.x,wp.y+44.f},{wp.x+W,wp.y+56.f},C(c::bg_panel));
        dl->AddLine({wp.x,wp.y+56.f},{wp.x+W,wp.y+56.f},C(c::stroke));
        dl->AddRectFilled(wp,{wp.x+W,wp.y+3.f},C(c::accent));
        // ícone
        dl->AddCircleFilled({wp.x+24.f,wp.y+28.f},13.f,C(c::accent,.08f));
        dl->AddCircle({wp.x+24.f,wp.y+28.f},10.f,C(c::accent,.40f),32,1.5f);
        dl->AddCircle({wp.x+24.f,wp.y+28.f}, 7.f,C(c::accent,.75f),32,1.5f);
        // nome
        ImGui::SetCursorPos({42.f,11.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text);   ImGui::Text("Oxygen"); ImGui::PopStyleColor();
        ImGui::SameLine(0,0);
        ImGui::PushStyleColor(ImGuiCol_Text,c::accent); ImGui::Text("Clicker"); ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SetCursorPos({42.f,30.f});
        ImGui::PushFont(font::regular);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("v1.0  ·  Linux AutoClicker"); ImGui::PopStyleColor();
        ImGui::PopFont();
        // badge
        bool on=g_clicker.enabled.load(), focused=(g_win!=0);
        ImVec4 bc; const char* bt;
        if(on&&focused){bc=c::green;bt="ACTIVE";}
        else if(on){bc=c::yellow;bt="NO FOCUS";}
        else{bc=c::red;bt="IDLE";}
        ImVec2 ts=ImGui::CalcTextSize(bt);
        float bx=wp.x+W-ts.x-24.f, by=wp.y+18.f;
        dl->AddRectFilled({bx-7.f,by-4.f},{bx+ts.x+7.f,by+ts.y+4.f},C(bc,.12f),5.f);
        dl->AddRect({bx-7.f,by-4.f},{bx+ts.x+7.f,by+ts.y+4.f},C(bc,.45f),5.f,0,1.f);
        dl->AddCircleFilled({bx-1.f,by+ts.y*.5f},3.f,C(bc));
        dl->AddText(font::regular,12.f,{bx+5.f,by},C(bc),bt);
    }

    ImGui::SetCursorPos({16.f,64.f});
    ImGui::BeginGroup();

    // ── AutoClicker + Bind na mesma row ───────────────────────────────────────
    Divider("AUTOCLICKER");
    {
        Card card(56.f);
        RowBegin(card.o,card.w,"Enable","Toggle clicking on/off");

        // Toggle
        float ty=card.o.y+(56.f-20.f)*.5f;
        ImGui::SetCursorScreenPos({card.o.x+card.w-54.f,ty});
        Toggle("##en",g_clicker.enabled);

        // Bind button — ao lado do toggle, estilo client-wl
        std::string bn=g_bind.name();
        ImVec4 bc=g_bind.waiting?c::yellow:c::accent;
        float btnW=60.f;
        ImGui::SetCursorScreenPos({card.o.x+card.w-54.f-btnW-6.f,ty-4.f});
        ImGui::PushFont(font::bold);
        ImGui::PushStyleColor(ImGuiCol_Button,       {bc.x,bc.y,bc.z,.13f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{bc.x,bc.y,bc.z,.25f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {bc.x,bc.y,bc.z,.40f});
        ImGui::PushStyleColor(ImGuiCol_Text,bc);
        ImGui::PushStyleColor(ImGuiCol_Border,{bc.x,bc.y,bc.z,.40f});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,5.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1.f);
        if(ImGui::Button(bn.c_str(),{btnW,28.f}))
            if(!g_bind.waiting) g_bind.waiting=true;
        ImGui::PopStyleVar(2); ImGui::PopStyleColor(5); ImGui::PopFont();

        // hint abaixo do bind quando waiting
        if(g_bind.waiting){
            ImGui::SetCursorScreenPos({card.o.x+card.w-54.f-btnW-6.f,ty+26.f});
            ImGui::PushFont(font::regular);
            ImGui::PushStyleColor(ImGuiCol_Text,c::yellow);
            ImGui::Text("Esc = clear");
            ImGui::PopStyleColor(); ImGui::PopFont();
        }
    }

    // ── CPS ───────────────────────────────────────────────────────────────────
    Divider("CLICKS PER SECOND");
    {
        Card card(112.f);
        float lo=g_clicker.minCPS.load(), hi=g_clicker.maxCPS.load();
        SliderRow("##lo","Min CPS",&lo,1.f,20.f,card.o.x+14.f,card.o.y+8.f,cw-28.f);
        if(lo!=g_clicker.minCPS.load()) g_clicker.minCPS.store(lo);
        SliderRow("##hi","Max CPS",&hi,1.f,20.f,card.o.x+14.f,card.o.y+58.f,cw-28.f);
        if(hi!=g_clicker.maxCPS.load()) g_clicker.maxCPS.store(hi);
        if(g_clicker.minCPS.load()>g_clicker.maxCPS.load())
            g_clicker.minCPS.store(g_clicker.maxCPS.load());
    }

    // ── Options ───────────────────────────────────────────────────────────────
    Divider("OPTIONS");
    {
        Card card(56.f);
        RowBegin(card.o,card.w,"Hold to click","Only click while LMB is held");
        ImGui::SetCursorScreenPos({card.o.x+card.w-54.f,card.o.y+(56.f-20.f)*.5f});
        Toggle("##hold",g_clicker.holdOnly);
    }

    ImGui::EndGroup();

    // ── Footer ────────────────────────────────────────────────────────────────
    {
        float fy=wp.y+H-22.f;
        dl->AddLine({wp.x+16.f,fy},{wp.x+W-16.f,fy},C(c::stroke,.35f));
        dl->AddText(font::regular,10.f,{wp.x+16.f,fy+6.f},C(c::text_dim,.25f),
                    "OxygenClicker  ·  github.com/henriquecarmellim/Oxygen");
    }
    ImGui::End();
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int,char**){
    XInitThreads();
    g_dpy_ui  = XOpenDisplay(nullptr);
    g_dpy_clk = XOpenDisplay(nullptr);
    if(!g_dpy_ui || !g_dpy_clk){
        fprintf(stderr,"XOpenDisplay failed: is DISPLAY set?\n");
        return 1;
    }

    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        fprintf(stderr,"SDL_Init error: %s\n",SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_Window* win=SDL_CreateWindow("OxygenClicker",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,360,430,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext ctx=SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io=ImGui::GetIO(); io.IniFilename=nullptr;

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
                g_bind.set(e.key.keysym.sym);
        }
        // Bind global: XGrabKey entrega KeyPress no g_dpy_ui mesmo sem foco
        if(g_dpy_ui && !g_bind.waiting && g_bind.key!=SDLK_UNKNOWN){
            while(XPending(g_dpy_ui)){
                XEvent xe; XNextEvent(g_dpy_ui,&xe);
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

    if(g_dpy_ui) XUngrabKey(g_dpy_ui,AnyKey,AnyModifier,DefaultRootWindow(g_dpy_ui));
    g_clicker.stop();
    if(g_dpy_ui)  XCloseDisplay(g_dpy_ui);
    if(g_dpy_clk) XCloseDisplay(g_dpy_clk);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
