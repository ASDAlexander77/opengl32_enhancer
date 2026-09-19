// Standalone config editor: a normal OpenGL app that renders a scene, runs the REAL
// ApplySelectedEffect() pipeline over it, and puts an ImGui UI next to the result so settings
// can be tuned and saved without launching the game.
//
// It edits the live AnaxConfig (see config.h's GetMutableAnaxConfig) that post_effects.cpp
// re-reads every frame, so every slider takes effect on the next frame with no reload step, and
// what is on screen is produced by the same code path the DLL runs in-game - not a preview that
// approximates it.
//
// Being a separate app rather than an in-game overlay is the whole point: it owns its window,
// message loop and GL context, so there is no WndProc subclassing inside someone else's process,
// no fighting the game for input, and no risk of destabilizing a running game to move a slider.
// The cost is that a synthetic scene is not the game's content - which is what the frame dump
// exists to fix. Press the dump hotkey in-game (frameDumpKey, F12 by default), then load the
// file here to tune against a real frame, depth and projection included.
#include <windows.h>
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"

#include "config.h"
#include "config_writer.h"
#include "editor_scene.h"
#include "gl_loader.h"
#include "post_effects.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

char g_iniPath[512] = "opengl32_enhancer.ini";
char g_dumpPath[512] = "opengl32_enhancer_frame.dump";
char g_status[512] = "";
bool g_useLoadedFrame = false;
bool g_showDepth = false;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

// Every stage name, in the order config.h declares them, for the "add a stage" combo.
const EffectKind kAllStages[] = {
    EffectKind::Bilinear, EffectKind::NVScaler, EffectKind::Fsr,
    EffectKind::Nr, EffectKind::Ssao, EffectKind::LocalContrast,
    EffectKind::Bloom, EffectKind::AcesToneMap, EffectKind::LutGrading,
    EffectKind::Vignette, EffectKind::DepthVignette, EffectKind::ChromaticAberration,
    EffectKind::Taa, EffectKind::Smaa, EffectKind::Cas, EffectKind::Sharpen,
    EffectKind::Dof, EffectKind::Fog, EffectKind::LightShafts, EffectKind::Ssr,
    EffectKind::Gamma, EffectKind::Dither, EffectKind::Invert,
};
const int kAllStageCount = (int)(sizeof(kAllStages) / sizeof(kAllStages[0]));

void DrawPipelineEditor(AnaxConfig& config) {
    ImGui::TextUnformatted("Stages run top to bottom. Order matters.");
    ImGui::Separator();

    int moveFrom = -1, moveTo = -1, removeAt = -1;
    for (int i = 0; i < config.stageCount; ++i) {
        ImGui::PushID(i);
        ImGui::Text("%2d.", i + 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180.0f);
        ImGui::TextUnformatted(EffectNameFor(config.stages[i]));
        ImGui::SameLine(240.0f);
        if (ImGui::ArrowButton("##up", ImGuiDir_Up) && i > 0) {
            moveFrom = i;
            moveTo = i - 1;
        }
        ImGui::SameLine();
        if (ImGui::ArrowButton("##down", ImGuiDir_Down) && i < config.stageCount - 1) {
            moveFrom = i;
            moveTo = i + 1;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("remove")) {
            removeAt = i;
        }
        ImGui::PopID();
    }

    // Applied after the loop so the list isn't mutated while it is being walked.
    if (moveFrom >= 0 && moveTo >= 0) {
        EffectKind tmp = config.stages[moveFrom];
        config.stages[moveFrom] = config.stages[moveTo];
        config.stages[moveTo] = tmp;
    }
    if (removeAt >= 0) {
        for (int i = removeAt; i < config.stageCount - 1; ++i) {
            config.stages[i] = config.stages[i + 1];
        }
        config.stageCount--;
    }

    ImGui::Separator();
    static int addIndex = 0;
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("##add", EffectNameFor(kAllStages[addIndex]))) {
        for (int i = 0; i < kAllStageCount; ++i) {
            bool selected = (addIndex == i);
            if (ImGui::Selectable(EffectNameFor(kAllStages[i]), selected)) {
                addIndex = i;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add stage")) {
        if (config.stageCount < kMaxEffectStages) {
            config.stages[config.stageCount++] = kAllStages[addIndex];
        }
    }
    if (config.stageCount == 0) {
        ImGui::TextDisabled("Pipeline is empty - the frame is shown unprocessed.");
    }
}

void DrawParameters(AnaxConfig& config) {
    // Slider ranges mirror config.cpp's clamps exactly, so the UI cannot produce a value the
    // parser would reject and silently rewrite on the next load.
    if (ImGui::CollapsingHeader("Scaling / sharpening")) {
        ImGui::SliderFloat("scale", &config.scale, 0.5f, 1.0f);
        ImGui::SliderFloat("sharpness", &config.sharpness, 0.0f, 1.0f);
        ImGui::Checkbox("fsrDenoise", &config.fsrDenoise);
        ImGui::SliderFloat("fsrFilmGrain", &config.fsrFilmGrain, 0.0f, 1.0f);
    }
    if (ImGui::CollapsingHeader("SSAO")) {
        if (!LoadedFrameHasProjection() && g_useLoadedFrame) {
            ImGui::TextDisabled("Loaded frame has no projection - ssao will no-op.");
        }
        ImGui::SliderFloat("ssaoRadius", &config.ssaoRadius, 0.1f, 512.0f, "%.1f");
        ImGui::TextDisabled("World units, not 0..1 - tune this against a real frame.");
        ImGui::SliderFloat("ssaoIntensity", &config.ssaoIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("ssaoBias", &config.ssaoBias, 0.0f, 16.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Bloom / tone / grade")) {
        ImGui::SliderFloat("bloomThreshold", &config.bloomThreshold, 0.0f, 1.0f);
        ImGui::SliderFloat("bloomIntensity", &config.bloomIntensity, 0.0f, 2.0f);
        ImGui::SliderFloat("acesStrength", &config.acesStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("lutStrength", &config.lutStrength, 0.0f, 1.0f);
        ImGui::InputText("lutPath", config.lutPath, sizeof(config.lutPath));
    }
    if (ImGui::CollapsingHeader("Lens")) {
        ImGui::SliderFloat("vignetteIntensity", &config.vignetteIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("vignetteRadius", &config.vignetteRadius, 0.0f, 1.0f);
        ImGui::SliderFloat("depthVignetteIntensity", &config.depthVignetteIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("depthVignetteThreshold", &config.depthVignetteThreshold, 0.0f, 1.0f);
        ImGui::SliderFloat("chromaticAberrationStrength", &config.chromaticAberrationStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("dofBlurStrength", &config.dofBlurStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("dofFocusDistance", &config.dofFocusDistance, 0.0f, 2048.0f);
        ImGui::SliderFloat("dofFocusRange", &config.dofFocusRange, 0.1f, 2048.0f);
        ImGui::TextDisabled("dof distances are in WORLD UNITS, like ssaoRadius - the loaded");
        ImGui::TextDisabled("frame's own range is shown under Scene. 0 distance = auto-focus.");
        ImGui::Separator();
        ImGui::SliderFloat("fogIntensity", &config.fogIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("fogStart", &config.fogStart, 0.0f, 4096.0f);
        ImGui::SliderFloat("fogEnd", &config.fogEnd, 0.0f, 8192.0f);
        float fogColor[3] = {config.fogColorR, config.fogColorG, config.fogColorB};
        if (ImGui::ColorEdit3("fogColor", fogColor)) {
            config.fogColorR = fogColor[0];
            config.fogColorG = fogColor[1];
            config.fogColorB = fogColor[2];
        }
        ImGui::TextDisabled("fog distances are in WORLD UNITS too. Unlike depthvignette, this");
        ImGui::TextDisabled("fades distance toward a COLOUR rather than toward black.");
        ImGui::Separator();
        ImGui::SliderFloat("shaftsIntensity", &config.shaftsIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("shaftsDensity", &config.shaftsDensity, 0.0f, 1.0f);
        ImGui::SliderFloat("shaftsDecay", &config.shaftsDecay, 0.0f, 1.0f);
        ImGui::SliderFloat("shaftsThreshold", &config.shaftsThreshold, 0.0f, 1.0f);
        ImGui::TextDisabled("lightshafts finds the light in the frame itself. Raise the");
        ImGui::TextDisabled("threshold if a bright wall is pulling the rays off the lamp.");
        ImGui::Separator();
        ImGui::SliderFloat("ssrIntensity", &config.ssrIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("ssrMaxDistance", &config.ssrMaxDistance, 1.0f, 4096.0f);
        ImGui::SliderFloat("ssrThickness", &config.ssrThickness, 0.1f, 256.0f);
        ImGui::SliderFloat("ssrUpThreshold", &config.ssrUpThreshold, 0.0f, 1.0f);
        ImGui::TextDisabled("ssr distances are in WORLD UNITS. ssrUpThreshold decides WHAT");
        ImGui::TextDisabled("reflects: a GL 1.1 game has no material channel, so only");
        ImGui::TextDisabled("upward-facing surfaces do. 1 gates everything off.");
        // ssrWorldUpAxis has no control here on purpose (see config_writer_test.cpp's
        // unmanaged-key test): it is a per-engine fact, not something tuned by eye, and this
        // editor could not help anyway - it has no captured camera, so the control would
        // visibly do nothing.
        //
        // motionBlurStrength/motionBlurMaxRadius are absent for the identical reason, one level
        // further up the call chain: editor_scene.cpp calls CaptureProjectionFrustum()
        // DIRECTLY rather than through the wrapper's glFrustum hook, so this editor never calls
        // NotifyWorldProjection()/NotifyWorldPassBegan() and therefore has no captured camera
        // and no world-only frame (see world_capture.h). GetCapturedCamera(), GetPreviousCamera()
        // and GetWorldOnlyFrame() all return false here, which makes motionblur provably no-op
        // in this editor - unlike ssrWorldUpAxis, though, these two ARE managed by the writer
        // (see config_writer.cpp), since a user editing them by hand must not lose them on save.
    }
    if (ImGui::CollapsingHeader("Anti-aliasing")) {
        ImGui::SliderFloat("taaBlend", &config.taaBlend, 0.0f, 1.0f);
        ImGui::SliderFloat("shimmerSuppression", &config.shimmerSuppression, 0.0f, 1.0f);
        ImGui::TextDisabled("taa blends across frames - it needs a moment to settle.");
    }
    if (ImGui::CollapsingHeader("Noise reduction / local contrast")) {
        ImGui::SliderFloat("nrIntensity", &config.nrIntensity, 0.0f, 1.0f);
        ImGui::SliderInt("nrPasses", &config.nrPasses, 1, 4);
        ImGui::SliderFloat("nrColorStrength", &config.nrColorStrength, 0.0f, 1.0f);
        ImGui::SliderFloat("nrTonePreservation", &config.nrTonePreservation, 0.0f, 1.0f);
        ImGui::SliderFloat("nrGrainPreservation", &config.nrGrainPreservation, 0.0f, 1.0f);
        ImGui::SliderFloat("localStructureStrength", &config.localStructureStrength, 0.0f, 2.0f);
        ImGui::SliderFloat("localToneStrength", &config.localToneStrength, 0.0f, 2.0f);
    }
    if (ImGui::CollapsingHeader("Gamma / brightness")) {
        ImGui::SliderFloat("gamma", &config.gamma, 0.5f, 3.0f);
        ImGui::SliderFloat("brightness", &config.brightness, 0.0f, 2.0f);
        ImGui::TextDisabled("1.0/1.0 is an exact no-op. brightness is a gain applied before");
        ImGui::TextDisabled("the exponent, so black stays black at every setting.");
        if (!HasEffectStage(config, EffectKind::Gamma)) {
            ImGui::TextDisabled("`gamma` is not in the pipeline above - these do nothing yet.");
        }
    }
    if (ImGui::CollapsingHeader("Output / misc")) {
        ImGui::SliderFloat("ditherStrength", &config.ditherStrength, 0.0f, 1.0f);
        ImGui::Checkbox("fxIndicator", &config.fxIndicator);
        ImGui::SliderFloat("anisotropy", &config.anisotropy, 0.0f, 16.0f, "%.0f");
        ImGui::TextDisabled("anisotropy and textureEffect act on the game's own textures,");
        ImGui::TextDisabled("so they have no visible effect in this editor.");
    }
}

void DrawSceneControls() {
    int dumpWidth = 0, dumpHeight = 0;
    LoadedFrameSize(dumpWidth, dumpHeight);

    if (ImGui::RadioButton("Synthetic scene", !g_useLoadedFrame)) {
        g_useLoadedFrame = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Loaded game frame", g_useLoadedFrame)) {
        if (HasLoadedFrame()) {
            g_useLoadedFrame = true;
        } else {
            snprintf(g_status, sizeof(g_status), "Load a frame dump first.");
        }
    }

    ImGui::InputText("dump path", g_dumpPath, sizeof(g_dumpPath));
    if (ImGui::Button("Load frame dump")) {
        if (LoadFrameDump(g_dumpPath)) {
            g_useLoadedFrame = true;
            LoadedFrameSize(dumpWidth, dumpHeight);
            snprintf(g_status, sizeof(g_status), "Loaded %s (%dx%d, projection=%s)",
                     g_dumpPath, dumpWidth, dumpHeight,
                     LoadedFrameHasProjection() ? "yes" : "no");
        } else {
            snprintf(g_status, sizeof(g_status), "Could not load %s - see console.", g_dumpPath);
        }
    }
    if (HasLoadedFrame()) {
        ImGui::Text("Loaded frame: %dx%d, projection %s",
                    dumpWidth, dumpHeight, LoadedFrameHasProjection() ? "yes" : "no");

        // The depth view shows the dump's depth plane instead of the post-processed result, so
        // it is a check on the INPUT the depth-consuming stages get - the one thing the ordinary
        // view cannot show, because depth only ever reaches the screen indirectly through ssao
        // and depthVignette.
        ImGui::Checkbox("Show depth map", &g_showDepth);

        LoadedFrameDepth depthInfo;
        if (GetLoadedFrameDepthInfo(depthInfo)) {
            if (!depthInfo.hasRange) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                                   "Depth is FLAT (%.6f everywhere) - ssao and depthVignette",
                                   depthInfo.minRaw);
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                                   "have nothing to work with. Re-dump from an in-game view.");
            } else if (depthInfo.nearUnits > 0.0f) {
                ImGui::Text("Depth: raw %.4f..%.4f", depthInfo.minRaw, depthInfo.maxRaw);
                // The number that matters for tuning: ssaoRadius is in world units, and this is
                // the world-unit span the frame actually covers.
                ImGui::Text("       %.0f..%.0f world units (ssaoRadius is in these)",
                            depthInfo.nearUnits, depthInfo.farUnits);
            } else {
                ImGui::Text("Depth: raw %.4f..%.4f", depthInfo.minRaw, depthInfo.maxRaw);
                ImGui::TextDisabled("       no projection in this dump, so no world-unit scale");
            }
        }
    } else {
        ImGui::TextDisabled("No frame loaded. In-game, press the frameDumpKey (F12 by default)");
        ImGui::TextDisabled("to write one next to the game's .exe, then point this at it.");
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        snprintf(g_iniPath, sizeof(g_iniPath), "%s", argv[1]);
    }

    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AnaxConfigEditorWindow";
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    if (!RegisterClassA(&wc)) {
        printf("FAILED: RegisterClassA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const int windowWidth = 1600, windowHeight = 900;
    RECT rect = {0, 0, windowWidth, windowHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "opengl32_enhancer - config editor",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd == nullptr) {
        printf("FAILED: CreateWindowExA, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    if (pixelFormat == 0 || !SetPixelFormat(hdc, pixelFormat, &pfd)) {
        printf("FAILED: ChoosePixelFormat/SetPixelFormat, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    HGLRC hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || !wglMakeCurrent(hdc, hglrc)) {
        printf("FAILED: wglCreateContext/wglMakeCurrent, GetLastError=%lu\n", GetLastError());
        return 1;
    }

    const GlComputeApi& gl = GetGlComputeApi();
    if (!gl.loaded) {
        printf("FAILED: this GPU/driver does not expose the GL 4.3 compute support every "
               "effect stage needs - there is nothing for the editor to show.\n");
        return 1;
    }

    // Load the ini into the same cached config object post_effects.cpp reads, so the sliders
    // below are editing exactly what the pipeline consults.
    AnaxConfig& config = GetMutableAnaxConfig();
    config = ParseConfigFile(g_iniPath);
    snprintf(g_dumpPath, sizeof(g_dumpPath), "%s", config.frameDumpPath);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_InitForOpenGL(hwnd);
    ImGui_ImplOpenGL3_Init("#version 430");

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        RECT client = {};
        GetClientRect(hwnd, &client);
        int clientWidth = client.right - client.left;
        int clientHeight = client.bottom - client.top;
        if (clientWidth <= 0 || clientHeight <= 0) {
            continue;
        }

        // Scene first, then the real pipeline over it, then the UI on top - so the UI is never
        // itself captured and post-processed, exactly as the DLL orders things in-game.
        // The depth view deliberately bypasses ApplySelectedEffect: it is a picture of the
        // frame's depth input, and running colour post-processing over it would say nothing
        // about either the depth or the effects.
        bool showingDepth = g_showDepth && g_useLoadedFrame && HasLoadedFrame();
        if (showingDepth) {
            RenderLoadedFrameDepth(clientWidth, clientHeight);
        } else {
            if (g_useLoadedFrame && HasLoadedFrame()) {
                RenderLoadedFrame(clientWidth, clientHeight);
            } else {
                RenderSyntheticScene(clientWidth, clientHeight);
            }

            ApplySelectedEffect(hdc);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(460, 840), ImGuiCond_FirstUseEver);
        ImGui::Begin("Config");

        if (ImGui::CollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawSceneControls();
        }
        if (ImGui::CollapsingHeader("Pipeline", ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawPipelineEditor(config);
        }
        DrawParameters(config);

        ImGui::Separator();
        ImGui::InputText("ini path", g_iniPath, sizeof(g_iniPath));
        if (ImGui::Button("Save to ini")) {
            if (WriteConfigToIni(g_iniPath, config)) {
                snprintf(g_status, sizeof(g_status), "Saved %s", g_iniPath);
            } else {
                snprintf(g_status, sizeof(g_status), "Could not save %s - see console.", g_iniPath);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload from ini")) {
            config = ParseConfigFile(g_iniPath);
            snprintf(g_status, sizeof(g_status), "Reloaded %s", g_iniPath);
        }
        if (g_status[0] != '\0') {
            ImGui::TextWrapped("%s", g_status);
        }
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    return 0;
}
