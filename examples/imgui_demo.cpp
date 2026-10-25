// bidi/examples/imgui_demo.cpp
//
// Demo for the bidi::imbidi::InputText widget.
// Uses SDL3 + OpenGL3 backend.
//
#include "bidi/imbidi/input_text.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <cstdio>

// =====================================================================
// Demo state
// =====================================================================
static bidi::imbidi::InputText g_single;
static bidi::imbidi::InputText g_multi(true);
static bool g_multi_on = true;

// =====================================================================
// Helper: radio buttons for base level
// =====================================================================
static bool BaseLevelRadio(const char* id,
    bidi::imbidi::BaseLevel& level) {
    ImGui::PushID(id);
    bidi::imbidi::BaseLevel prev = level;
    if (ImGui::RadioButton("Auto", level == bidi::imbidi::BaseLevel::Auto))
        level = bidi::imbidi::BaseLevel::Auto;
    ImGui::SameLine();
    if (ImGui::RadioButton("LTR", level == bidi::imbidi::BaseLevel::LTR))
        level = bidi::imbidi::BaseLevel::LTR;
    ImGui::SameLine();
    if (ImGui::RadioButton("RTL", level == bidi::imbidi::BaseLevel::RTL))
        level = bidi::imbidi::BaseLevel::RTL;
    ImGui::PopID();
    return level != prev;
}

// =====================================================================
// Demo UI
// =====================================================================
static void DrawDemo() {
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Bidi Input Test")) {
        ImGui::TextUnformatted(
            "Type Persian / Arabic / Hebrew mixed with English.");

        // =================================================================
        // Single-line
        // =================================================================
        ImGui::SeparatorText("Single-line");

        static bidi::imbidi::BaseLevel single_level =
            bidi::imbidi::BaseLevel::Auto;
        if (BaseLevelRadio("##single_radio", single_level))
            g_single.setBaseLevel(single_level);

        g_single.render("##single", ImVec2(960, 0));

        ImGui::Text("Cursor: %zu | Sel: %zu..%zu | Focused: %d",
            g_single.cursor(),
            g_single.selectionStart(),
            g_single.selectionEnd(),
            g_single.focused() ? 1 : 0);

        if (ImGui::Button("Sample (single)"))
            g_single.setText("Dear ImGui suport RTL. پارسی English. این یک نوشته پارسی آزمایشی است.");
        ImGui::SameLine();
        if (ImGui::Button("Clear (single)"))
            g_single.setText("");

        // =================================================================
        // Multiline
        // =================================================================
        ImGui::SeparatorText("Multiline");

        static bidi::imbidi::BaseLevel multi_level =
            bidi::imbidi::BaseLevel::Auto;
        if (BaseLevelRadio("##multi_radio", multi_level))
            g_multi.setBaseLevel(multi_level);

        if (ImGui::Checkbox("Multiline enabled", &g_multi_on))
            g_multi.setMultiline(g_multi_on);

        ImVec2 multi_size = g_multi_on ? ImVec2(960, 150) : ImVec2(960, 0);
        g_multi.render("##multi", multi_size);

        ImGui::Text("Cursor: %zu | Sel: %zu..%zu | Focused: %d",
            g_multi.cursor(),
            g_multi.selectionStart(),
            g_multi.selectionEnd(),
            g_multi.focused() ? 1 : 0);

        if (ImGui::Button("Sample (multi)"))
            g_multi.setText("پارسی را پاس بداریم.\nشما میتوانید پارسی بنویسید.\nאפשר גם לכתוב בעברית.\nيمكنك أيضاً الكتابة باللغة العربية.\nOf course, you can also write in English.\nدرود Hello السلام عليكم שלום עליכם");
        ImGui::SameLine();
        if (ImGui::Button("Clear (multi)"))
            g_multi.setText("");
    }
    ImGui::End();
}

// =====================================================================
// Main (SDL3 + OpenGL3)
// =====================================================================
int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_HIDDEN |
        SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = SDL_CreateWindow(
        "Bidi Input Demo (SDL3 + OpenGL3)",
        (int)(1050 * main_scale), (int)(750 * main_scale),
        window_flags);
    if (!window) {
        std::printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        std::printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Font with Persian support.
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    static const ImWchar font_ranges[] = {
        0x0020, 0x00FF,
        0x2000, 0x206F,
        0x0600, 0x06FF,
        0xFB50, 0xFDFF,
        0xFE70, 0xFEFF,
        0
    };
    const char* font_path = "C:/Windows/Fonts/tahoma.ttf";
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        font_path, 32.0f, &font_cfg, font_ranges);
    if (!font) {
        std::printf("Warning: could not load '%s'.\n", font_path);
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initial content.
    g_single.setText("Dear ImGui suport RTL. پارسی English. این یک نوشته پارسی آزمایشی است.");

    g_multi.setText("پارسی را پاس بداریم.\nشما میتوانید پارسی بنویسید.\nאפשר גם לכתוב בעברית.\nيمكنك أيضاً الكتابة باللغة العربية.\nOf course, you can also write in English.\nدرود Hello السلام عليكم שלום עליכם");

    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        DrawDemo();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}