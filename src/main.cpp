#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <cstring>
#include <cstdio>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Palette
// ─────────────────────────────────────────────────────────────────────────────
namespace c {
    constexpr ImVec4 accent     = {0.52f, 0.38f, 0.90f, 1.f};
    constexpr ImVec4 accent_dim = {0.34f, 0.24f, 0.64f, 1.f};
    constexpr ImVec4 bg         = {0.038f,0.038f,0.050f,1.f};
    constexpr ImVec4 bg_panel   = {0.055f,0.055f,0.072f,1.f};
    constexpr ImVec4 bg_widget  = {0.080f,0.078f,0.105f,1.f};
    constexpr ImVec4 stroke     = {0.120f,0.110f,0.165f,1.f};
    constexpr ImVec4 text       = {0.870f,0.850f,0.950f,1.f};
    constexpr ImVec4 text_dim   = {0.360f,0.340f,0.450f,1.f};
    constexpr ImVec4 green      = {0.310f,0.780f,0.470f,1.f};
    constexpr ImVec4 red        = {0.780f,0.270f,0.270f,1.f};
    constexpr ImVec4 yellow     = {0.90f, 0.75f, 0.20f, 1.f};
}
static ImU32 C(ImVec4 v, float a = 1.f) {
    return IM_COL32(int(v.x*255),int(v.y*255),int(v.z*255),int(a*255));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bind system  (X11 KeySym)
// ─────────────────────────────────────────────────────────────────────────────
struct Bind {
    KeySym  sym      = NoSymbol;   // tecla configurada
    bool    waiting  = false;      // aguardando input do usuário
    bool    prevHeld = false;      // estado anterior (edge detection)

    // Retorna nome legível
    std::string name() const {
        if (waiting)  return "...";
        if (sym == NoSymbol) return "None";
        const char* n = XKeysymToString(sym);
        return n ? n : "?";
    }

    // Chama a cada frame — retorna true no rising edge da tecla
    bool poll(Display* dpy) {
        if (sym == NoSymbol || waiting) return false;
        KeyCode kc = XKeysymToKeycode(dpy, sym);
        if (!kc) return false;
        char keys[32]; XQueryKeymap(dpy, keys);
        bool held = (keys[kc/8] >> (kc%8)) & 1;
        bool edge = held && !prevHeld;
        prevHeld  = held;
        return edge;
    }

    // Tenta capturar a próxima tecla pressionada — retorna true quando capturou
    bool capture(Display* dpy) {
        if (!waiting) return false;
        char keys[32]; XQueryKeymap(dpy, keys);
        for (int kc = 8; kc < 256; kc++) {
            if (!((keys[kc/8] >> (kc%8)) & 1)) continue;
            KeySym ks = XkbKeycodeToKeysym(dpy, kc, 0, 0);
            // ignora modificadores puros
            if (ks == XK_Shift_L || ks == XK_Shift_R ||
                ks == XK_Control_L || ks == XK_Control_R ||
                ks == XK_Alt_L || ks == XK_Alt_R ||
                ks == XK_Super_L || ks == XK_Super_R) continue;
            // Escape = limpar bind
            if (ks == XK_Escape) { sym = NoSymbol; waiting = false; return true; }
            sym     = ks;
            waiting = false;
            return true;
        }
        return false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  AutoClicker
// ─────────────────────────────────────────────────────────────────────────────
struct Clicker {
    std::atomic<bool>  enabled {false};
    std::atomic<float> minCPS  {10.f};
    std::atomic<float> maxCPS  {14.f};
    std::atomic<bool>  holdOnly{true};
    std::atomic<bool>  running {true};

    Display*    dpy    = nullptr;
    std::thread worker;

    void start() {
        dpy    = XOpenDisplay(nullptr);
        worker = std::thread([this]{ loop(); });
    }
    void stop() {
        running = false;
        if (worker.joinable()) worker.join();
        if (dpy) { XCloseDisplay(dpy); dpy = nullptr; }
    }

private:
    bool lmbHeld() {
        if (!dpy) return false;
        Window r,ch; int rx,ry,wx,wy; unsigned int mask;
        XQueryPointer(dpy, DefaultRootWindow(dpy), &r,&ch,&rx,&ry,&wx,&wy,&mask);
        return mask & Button1Mask;
    }
    void loop() {
        std::mt19937 rng{std::random_device{}()};
        auto last = std::chrono::steady_clock::now();
        long long delay = 80;
        while (running) {
            if (enabled && (!holdOnly || lmbHeld())) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now-last).count() >= delay) {
                    XTestFakeButtonEvent(dpy, 1, True,  CurrentTime);
                    XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
                    XFlush(dpy);
                    last = now;
                    float lo = minCPS.load(), hi = maxCPS.load();
                    if (lo > hi) lo = hi;
                    delay = (long long)(1000.f / std::uniform_real_distribution<float>(lo,hi)(rng));
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
} g_clicker;

// Bind vive na thread principal (acessa Display da UI)
static Bind  g_bind;
static Display* g_dpy_ui = nullptr; // display exclusivo da thread UI

// ─────────────────────────────────────────────────────────────────────────────
//  Style
// ─────────────────────────────────────────────────────────────────────────────
static void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 10.f; s.ChildRounding    = 7.f;
    s.FrameRounding    = 5.f;  s.GrabRounding     = 5.f;
    s.PopupRounding    = 5.f;  s.ScrollbarRounding = 5.f;
    s.WindowBorderSize = 1.f;  s.FrameBorderSize  = 0.f;
    s.ItemSpacing      = {8.f, 7.f};
    s.FramePadding     = {10.f, 5.f};
    s.WindowPadding    = {16.f, 16.f};

    auto* col = s.Colors;
    col[ImGuiCol_WindowBg]             = c::bg;
    col[ImGuiCol_ChildBg]              = c::bg_panel;
    col[ImGuiCol_Border]               = c::stroke;
    col[ImGuiCol_FrameBg]              = c::bg_widget;
    col[ImGuiCol_FrameBgHovered]       = {0.11f,0.10f,0.15f,1.f};
    col[ImGuiCol_FrameBgActive]        = {0.15f,0.13f,0.20f,1.f};
    col[ImGuiCol_SliderGrab]           = c::accent;
    col[ImGuiCol_SliderGrabActive]     = c::accent_dim;
    col[ImGuiCol_CheckMark]            = c::accent;
    col[ImGuiCol_Button]               = c::bg_widget;
    col[ImGuiCol_ButtonHovered]        = {0.13f,0.11f,0.19f,1.f};
    col[ImGuiCol_ButtonActive]         = c::accent_dim;
    col[ImGuiCol_Text]                 = c::text;
    col[ImGuiCol_TextDisabled]         = c::text_dim;
    col[ImGuiCol_TitleBg]              = c::bg;
    col[ImGuiCol_TitleBgActive]        = c::bg;
    col[ImGuiCol_ScrollbarBg]          = c::bg_panel;
    col[ImGuiCol_ScrollbarGrab]        = c::stroke;
    col[ImGuiCol_ScrollbarGrabHovered] = c::accent_dim;
    col[ImGuiCol_ScrollbarGrabActive]  = c::accent;
    col[ImGuiCol_PopupBg]              = c::bg_panel;
    col[ImGuiCol_Separator]            = c::stroke;
    col[ImGuiCol_Header]               = c::bg_widget;
    col[ImGuiCol_HeaderHovered]        = {0.13f,0.11f,0.19f,1.f};
    col[ImGuiCol_HeaderActive]         = c::accent_dim;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Widgets
// ─────────────────────────────────────────────────────────────────────────────
static void Toggle(const char* id, std::atomic<bool>& val) {
    bool v = val.load();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float W=38.f, H=20.f, R=H*.5f;
    ImGui::InvisibleButton(id, {W,H});
    if (ImGui::IsItemClicked()) { v=!v; val.store(v); }
    bool hov = ImGui::IsItemHovered();
    ImU32 bg  = v ? C(c::accent, hov?.85f:1.f) : C(c::bg_widget);
    ImU32 brd = v ? C(c::accent,.4f) : C(c::stroke);
    dl->AddRectFilled(p,{p.x+W,p.y+H},bg,R);
    dl->AddRect(p,{p.x+W,p.y+H},brd,R,0,1.f);
    float cx = p.x+R+(v?W-H:0.f);
    dl->AddCircleFilled({cx,p.y+R},R-3.f,IM_COL32(255,255,255,235));
}

static void SectionLabel(const char* label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float  w = ImGui::GetContentRegionAvail().x;
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddLine({p.x,p.y+ts.y*.5f},{p.x+8.f,p.y+ts.y*.5f},C(c::stroke));
    dl->AddText({p.x+12.f,p.y},C(c::text_dim),label);
    dl->AddLine({p.x+16.f+ts.x,p.y+ts.y*.5f},{p.x+w,p.y+ts.y*.5f},C(c::stroke));
    ImGui::Dummy({w,ts.y+4.f});
}

struct Card {
    ImVec2 origin; float width, height;
    Card(float h) : height(h) {
        origin = ImGui::GetCursorScreenPos();
        width  = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(origin,{origin.x+width,origin.y+height},C(c::bg_panel),7.f);
        dl->AddRect      (origin,{origin.x+width,origin.y+height},C(c::stroke),  7.f,0,1.f);
        dl->AddRectFilled(origin,{origin.x+width,origin.y+2.f},   C(c::accent,.7f),7.f);
        ImGui::SetCursorScreenPos({origin.x+14.f,origin.y+12.f});
        ImGui::PushClipRect(origin,{origin.x+width,origin.y+height},true);
    }
    ~Card() {
        ImGui::PopClipRect();
        ImGui::SetCursorScreenPos({origin.x, origin.y+height+8.f});
        ImGui::Dummy({0.f,0.f});
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  UI
// ─────────────────────────────────────────────────────────────────────────────
static void RenderUI() {
    constexpr float W=370.f, H=510.f;
    ImGuiIO& io = ImGui::GetIO();

    // Janela sempre centrada e não movível
    ImGui::SetNextWindowPos({(io.DisplaySize.x-W)*.5f,(io.DisplaySize.y-H)*.5f},
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize({W,H}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.f);

    ImGui::Begin("##oxy", nullptr,
        ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize      |
        ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoScrollbar   |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    // ── Header ────────────────────────────────────────────────────────────────
    {
        dl->AddRectFilled(wp,{wp.x+W,wp.y+56.f},C(c::bg_panel),10.f);
        dl->AddRectFilled({wp.x,wp.y+46.f},{wp.x+W,wp.y+56.f},C(c::bg_panel));
        dl->AddLine({wp.x,wp.y+56.f},{wp.x+W,wp.y+56.f},C(c::stroke));
        dl->AddRectFilled(wp,{wp.x+W,wp.y+2.f},C(c::accent));

        dl->AddCircle({wp.x+26.f,wp.y+28.f},12.f,C(c::accent,.15f),32,8.f);
        dl->AddCircle({wp.x+26.f,wp.y+28.f},10.f,C(c::accent,.5f), 32,1.5f);

        ImGui::SetCursorPos({42.f,16.f});
        ImGui::PushStyleColor(ImGuiCol_Text,c::text);   ImGui::Text("Oxygen"); ImGui::PopStyleColor();
        ImGui::SameLine(0,0);
        ImGui::PushStyleColor(ImGuiCol_Text,c::accent); ImGui::Text("Clicker"); ImGui::PopStyleColor();
        ImGui::SetCursorPos({42.f,34.f});
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("v1.0  |  Linux AutoClicker"); ImGui::PopStyleColor();

        bool on = g_clicker.enabled.load();
        ImVec4 bc = on ? c::green : c::red;
        const char* bt = on ? "ACTIVE" : "IDLE";
        ImVec2 ts = ImGui::CalcTextSize(bt);
        float bx=wp.x+W-ts.x-26.f, by=wp.y+20.f;
        dl->AddRectFilled({bx-7.f,by-4.f},{bx+ts.x+7.f,by+ts.y+4.f},C(bc,.12f),5.f);
        dl->AddRect      ({bx-7.f,by-4.f},{bx+ts.x+7.f,by+ts.y+4.f},C(bc,.45f),5.f,0,1.f);
        dl->AddText({bx,by},C(bc),bt);
    }

    ImGui::SetCursorPos({16.f,68.f});
    ImGui::BeginGroup();
    float cw = W-32.f;

    // ── Enable ────────────────────────────────────────────────────────────────
    SectionLabel("AUTOCLICKER");
    {
        Card card(58.f);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text);    ImGui::Text("Enable AutoClicker"); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+14.f,card.origin.y+32.f});
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Toggle clicking on/off"); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+card.width-52.f,card.origin.y+19.f});
        Toggle("##en", g_clicker.enabled);
    }

    // ── CPS ───────────────────────────────────────────────────────────────────
    SectionLabel("CLICKS PER SECOND");
    {
        Card card(118.f);
        // Min
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Min CPS"); ImGui::PopStyleColor();
        ImGui::SameLine(cw-36.f);
        ImGui::PushStyleColor(ImGuiCol_Text,c::accent); ImGui::Text("%.1f",g_clicker.minCPS.load()); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+14.f,card.origin.y+34.f});
        ImGui::PushItemWidth(cw-28.f);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,c::accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,c::accent_dim);
        float lo=g_clicker.minCPS.load();
        if (ImGui::SliderFloat("##lo",&lo,1.f,20.f,"")) g_clicker.minCPS.store(lo);
        ImGui::PopStyleColor(2); ImGui::PopItemWidth();
        // Max
        ImGui::SetCursorScreenPos({card.origin.x+14.f,card.origin.y+62.f});
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Max CPS"); ImGui::PopStyleColor();
        ImGui::SameLine(cw-36.f);
        ImGui::PushStyleColor(ImGuiCol_Text,c::accent); ImGui::Text("%.1f",g_clicker.maxCPS.load()); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+14.f,card.origin.y+84.f});
        ImGui::PushItemWidth(cw-28.f);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab,c::accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,c::accent_dim);
        float hi=g_clicker.maxCPS.load();
        if (ImGui::SliderFloat("##hi",&hi,1.f,20.f,"")) g_clicker.maxCPS.store(hi);
        ImGui::PopStyleColor(2); ImGui::PopItemWidth();
        if (g_clicker.minCPS.load()>g_clicker.maxCPS.load()) g_clicker.minCPS.store(g_clicker.maxCPS.load());
    }

    // ── Options ───────────────────────────────────────────────────────────────
    SectionLabel("OPTIONS");
    {
        Card card(58.f);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text);    ImGui::Text("Hold to click"); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+14.f,card.origin.y+32.f});
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Only click while LMB is held"); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+card.width-52.f,card.origin.y+19.f});
        Toggle("##hold", g_clicker.holdOnly);
    }

    // ── Bind ──────────────────────────────────────────────────────────────────
    SectionLabel("KEYBIND");
    {
        // Captura tecla se waiting
        if (g_bind.waiting && g_dpy_ui)
            g_bind.capture(g_dpy_ui);

        // Poll bind → toggle clicker
        if (g_dpy_ui && g_bind.poll(g_dpy_ui))
            g_clicker.enabled.store(!g_clicker.enabled.load());

        Card card(58.f);
        ImGui::PushStyleColor(ImGuiCol_Text,c::text);    ImGui::Text("Toggle key"); ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos({card.origin.x+14.f,card.origin.y+32.f});
        ImGui::PushStyleColor(ImGuiCol_Text,c::text_dim); ImGui::Text("Press to toggle AutoClicker"); ImGui::PopStyleColor();

        // Botão de bind
        std::string bname = g_bind.name();
        ImVec4 btnCol = g_bind.waiting ? c::yellow : c::accent;
        ImGui::SetCursorScreenPos({card.origin.x+card.width-90.f, card.origin.y+16.f});
        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(btnCol.x,btnCol.y,btnCol.z,.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(btnCol.x,btnCol.y,btnCol.z,.30f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(btnCol.x,btnCol.y,btnCol.z,.45f));
        ImGui::PushStyleColor(ImGuiCol_Text, btnCol);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f);
        if (ImGui::Button(bname.c_str(), {76.f, 26.f})) {
            if (!g_bind.waiting) g_bind.waiting = true;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    // ── CPS bar ───────────────────────────────────────────────────────────────
    {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float h=28.f;
        dl->AddRectFilled(cp,{cp.x+cw,cp.y+h},C(c::bg_panel),6.f);
        dl->AddRect      (cp,{cp.x+cw,cp.y+h},C(c::stroke),  6.f,0,1.f);
        float lo=g_clicker.minCPS.load(), hi=g_clicker.maxCPS.load();
        float mid=(lo+hi)*.5f, pct=(mid-1.f)/19.f, bw=(cw-24.f)*pct;
        dl->AddRectFilled({cp.x+12.f,cp.y+10.f},{cp.x+12.f+cw-24.f,cp.y+18.f},C(c::bg_widget),3.f);
        if (bw>0.f) dl->AddRectFilled({cp.x+12.f,cp.y+10.f},{cp.x+12.f+bw,cp.y+18.f},C(c::accent),3.f);
        char buf[32]; snprintf(buf,sizeof(buf),"~%.1f CPS",mid);
        ImVec2 ts=ImGui::CalcTextSize(buf);
        dl->AddText({cp.x+cw-ts.x-12.f,cp.y+7.f},C(c::text_dim),buf);
        ImGui::Dummy({cw,h+8.f});
    }

    ImGui::EndGroup();

    // ── Footer ────────────────────────────────────────────────────────────────
    {
        float fy=wp.y+H-26.f;
        dl->AddLine({wp.x+16.f,fy},{wp.x+W-16.f,fy},C(c::stroke));
        dl->AddText({wp.x+16.f,fy+7.f},C(c::text_dim,.45f),
                    "OxygenClicker  |  github.com/revann/OxygenClicker");
    }

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────
int main(int, char**) {
    XInitThreads();
    g_dpy_ui = XOpenDisplay(nullptr);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_Window* win = SDL_CreateWindow(
        "OxygenClicker",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        370, 510,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;

    ApplyStyle();
    ImGui_ImplSDL2_InitForOpenGL(win, ctx);
    ImGui_ImplOpenGL3_Init("#version 130");

    g_clicker.start();

    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        auto& io = ImGui::GetIO();
        glViewport(0,0,(int)io.DisplaySize.x,(int)io.DisplaySize.y);
        glClearColor(c::bg.x,c::bg.y,c::bg.z,1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    g_clicker.stop();
    if (g_dpy_ui) XCloseDisplay(g_dpy_ui);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
