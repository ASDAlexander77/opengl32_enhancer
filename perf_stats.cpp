// See perf_stats.h.
#include "perf_stats.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "gl_loader.h"

// Same no-<windows.h> discipline as config.cpp - see the comment there.
extern "C" {
    __declspec(dllimport) int __stdcall QueryPerformanceCounter(long long* count);
    __declspec(dllimport) int __stdcall QueryPerformanceFrequency(long long* frequency);
    __declspec(dllimport) void* __stdcall WindowFromDC(void* hdc);
    __declspec(dllimport) int __stdcall GetWindowTextA(void* hwnd, char* text, int maxCount);
    __declspec(dllimport) int __stdcall SetWindowTextA(void* hwnd, const char* text);
}

namespace {

const unsigned int GL_TIMESTAMP              = 0x8E28;
const unsigned int GL_QUERY_RESULT           = 0x8866;
const unsigned int GL_QUERY_RESULT_AVAILABLE = 0x8867;

const double kReportIntervalMs = 2000.0;

// Frames in flight before a slot's queries are read. Four is comfortably more than any driver
// queues, so the results are virtually always ready; if one is not, that frame is dropped from
// the averages rather than waited for, because waiting is exactly the stall being measured.
const int kSlots = 4;
const int kMaxMarks = 64;

struct Slot {
    unsigned int queries[kMaxMarks];
    const char* labels[kMaxMarks];
    int count = 0;
    bool pending = false;
};

struct Segment {
    const char* label;
    double gpuMs;
    int frames;
};

struct State {
    unsigned int generation = 0;
    bool queriesMade = false;
    Slot slots[kSlots];
    int current = 0;
    bool frameOpen = false;

    Segment segments[kMaxMarks];
    int segmentCount = 0;
    int gpuFrames = 0;
    int gpuDropped = 0;

    // CPU, accumulated over the report window.
    double windowStartMs = 0.0;
    double frameBeginMs = 0.0;     // after the previous swap returned
    double postBeginMs = 0.0;      // first PerfGpuMark of the frame ("game" ends)
    double swapBeginMs = 0.0;      // PerfFrameEnd
    double sumGameCpuMs = 0.0;
    double sumPostCpuMs = 0.0;
    double sumSwapMs = 0.0;
    double maxFrameMs = 0.0;
    double lastFrameBeginMs = 0.0;
    int frames = 0;

    long long draws = 0;
    int uploads = 0;
    int effectUploads = 0;
    double effectUploadMs = 0.0;

    char baseTitle[128] = {0};
    void* titledWindow = nullptr;
};

State g_state;

double Frequency() {
    static double perMs = 0.0;
    if (perMs == 0.0) {
        long long f = 0;
        QueryPerformanceFrequency(&f);
        perMs = (double)f / 1000.0;
    }
    return perMs;
}

bool Enabled() {
    return GetAnaxConfig().debugStats;
}

// Queries belong to the context that made them. On a change, the old names are dropped, not
// deleted - see GetGlContextGeneration() in gl_loader.h.
bool EnsureQueries(const GlComputeApi& gl) {
    if (!gl.loaded || gl.glGenQueries == nullptr || gl.glQueryCounter == nullptr ||
        gl.glGetQueryObjectiv == nullptr || gl.glGetQueryObjectui64v == nullptr) {
        return false;
    }
    if (g_state.queriesMade && g_state.generation == GetGlContextGeneration()) {
        return true;
    }
    for (int i = 0; i < kSlots; ++i) {
        gl.glGenQueries(kMaxMarks, g_state.slots[i].queries);
        g_state.slots[i].count = 0;
        g_state.slots[i].pending = false;
    }
    g_state.generation = GetGlContextGeneration();
    g_state.queriesMade = true;
    g_state.frameOpen = false;
    return true;
}

void AddSegment(const char* label, double ms) {
    for (int i = 0; i < g_state.segmentCount; ++i) {
        if (g_state.segments[i].label == label || strcmp(g_state.segments[i].label, label) == 0) {
            g_state.segments[i].gpuMs += ms;
            g_state.segments[i].frames += 1;
            return;
        }
    }
    if (g_state.segmentCount < kMaxMarks) {
        g_state.segments[g_state.segmentCount++] = Segment{label, ms, 1};
    }
}

// Reads a finished slot into the accumulators, if its results are ready - never waits.
void Harvest(const GlComputeApi& gl, Slot& slot) {
    if (!slot.pending) {
        return;
    }
    slot.pending = false;
    if (slot.count < 2) {
        return;
    }
    int available = 0;
    gl.glGetQueryObjectiv(slot.queries[slot.count - 1], GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) {
        ++g_state.gpuDropped;
        return;
    }
    unsigned long long previous = 0;
    gl.glGetQueryObjectui64v(slot.queries[0], GL_QUERY_RESULT, &previous);
    for (int i = 1; i < slot.count; ++i) {
        unsigned long long t = 0;
        gl.glGetQueryObjectui64v(slot.queries[i], GL_QUERY_RESULT, &t);
        double ms = (t >= previous) ? (double)(t - previous) / 1.0e6 : 0.0;
        AddSegment(slot.labels[i], ms);
        previous = t;
    }
    ++g_state.gpuFrames;
}

void OpenFrame(const GlComputeApi& gl) {
    g_state.current = (g_state.current + 1) % kSlots;
    Slot& slot = g_state.slots[g_state.current];
    Harvest(gl, slot);     // the slot being reused is kSlots frames old
    slot.count = 0;
    slot.labels[0] = "frame start";
    gl.glQueryCounter(slot.queries[0], GL_TIMESTAMP);
    slot.count = 1;
    g_state.frameOpen = true;
}

void Report(void* hdc) {
    double now = PerfNowMs();
    double elapsed = now - g_state.windowStartMs;
    int frames = g_state.frames > 0 ? g_state.frames : 1;
    double fps = g_state.frames * 1000.0 / (elapsed > 0.0 ? elapsed : 1.0);
    double frameMs = elapsed / frames;

    char gpu[1024];
    int len = 0;
    double gpuTotal = 0.0;
    double effectsTotal = 0.0;
    const char* topLabel = "-";
    double topMs = 0.0;
    int gpuFrames = g_state.gpuFrames > 0 ? g_state.gpuFrames : 1;
    gpu[0] = 0;
    for (int i = 0; i < g_state.segmentCount; ++i) {
        const Segment& s = g_state.segments[i];
        double avg = s.gpuMs / gpuFrames;
        gpuTotal += avg;
        if (strcmp(s.label, "game") != 0) {
            effectsTotal += avg;
            if (avg > topMs) {
                topMs = avg;
                topLabel = s.label;
            }
        }
        if (len < (int)sizeof(gpu) - 64) {
            len += snprintf(gpu + len, sizeof(gpu) - len, "%s%s %.2f", i ? ", " : "", s.label, avg);
        }
    }

    printf("[opengl32_enh_cpp] stats: %.1f fps, %.2f ms/frame (worst %.1f) | CPU ms/frame: game %.2f, "
           "effects %.2f, swap wait %.2f | GPU ms/frame %.2f (effects %.2f): %s | draws %lld/frame, "
           "uploads %d (textureEffect %d, %.1f ms CPU)%s\n",
           fps, frameMs, g_state.maxFrameMs, g_state.sumGameCpuMs / frames,
           g_state.sumPostCpuMs / frames, g_state.sumSwapMs / frames, gpuTotal, effectsTotal,
           g_state.segmentCount ? gpu : "(no timer results yet)", g_state.draws / frames,
           g_state.uploads, g_state.effectUploads, g_state.effectUploadMs,
           g_state.gpuDropped ? " [some GPU samples not ready, dropped]" : "");
    fflush(stdout);

    // The window title: the one place the numbers are visible while playing.
    void* hwnd = hdc ? WindowFromDC(hdc) : nullptr;
    if (hwnd != nullptr) {
        if (hwnd != g_state.titledWindow) {
            GetWindowTextA(hwnd, g_state.baseTitle, (int)sizeof(g_state.baseTitle));
            g_state.titledWindow = hwnd;
        }
        char title[256];
        snprintf(title, sizeof(title), "%s | %.0f fps %.1f ms | GPU %.1f (fx %.1f, top %s %.1f) | swap wait %.1f",
                 g_state.baseTitle, fps, frameMs, gpuTotal, effectsTotal, topLabel, topMs,
                 g_state.sumSwapMs / frames);
        SetWindowTextA(hwnd, title);
    }

    g_state.windowStartMs = now;
    g_state.segmentCount = 0;
    g_state.gpuFrames = 0;
    g_state.gpuDropped = 0;
    g_state.sumGameCpuMs = g_state.sumPostCpuMs = g_state.sumSwapMs = 0.0;
    g_state.maxFrameMs = 0.0;
    g_state.frames = 0;
    g_state.draws = 0;
    g_state.uploads = 0;
    g_state.effectUploads = 0;
    g_state.effectUploadMs = 0.0;
}

}  // namespace

double PerfNowMs() {
    long long c = 0;
    QueryPerformanceCounter(&c);
    return (double)c / Frequency();
}

void PerfFrameBegin() {
    if (!Enabled()) {
        return;
    }
    double now = PerfNowMs();
    if (g_state.swapBeginMs > 0.0) {
        g_state.sumSwapMs += now - g_state.swapBeginMs;
    }
    if (g_state.lastFrameBeginMs > 0.0) {
        double frameMs = now - g_state.lastFrameBeginMs;
        if (frameMs > g_state.maxFrameMs) {
            g_state.maxFrameMs = frameMs;
        }
    }
    g_state.lastFrameBeginMs = now;
    g_state.frameBeginMs = now;
    g_state.postBeginMs = 0.0;

    const GlComputeApi& gl = GetGlComputeApi();
    if (EnsureQueries(gl)) {
        OpenFrame(gl);
    }
}

void PerfGpuMark(const char* label) {
    if (!Enabled()) {
        return;
    }
    if (g_state.postBeginMs == 0.0) {
        g_state.postBeginMs = PerfNowMs();
    }
    const GlComputeApi& gl = GetGlComputeApi();
    if (!EnsureQueries(gl)) {
        return;
    }
    if (!g_state.frameOpen) {
        OpenFrame(gl);
    }
    Slot& slot = g_state.slots[g_state.current];
    if (slot.count >= kMaxMarks) {
        return;
    }
    slot.labels[slot.count] = label;
    gl.glQueryCounter(slot.queries[slot.count], GL_TIMESTAMP);
    ++slot.count;
}

void PerfFrameEnd(void* hdc) {
    if (!Enabled()) {
        return;
    }
    double now = PerfNowMs();
    if (g_state.windowStartMs == 0.0) {
        g_state.windowStartMs = now;
    }
    if (g_state.frameBeginMs > 0.0) {
        double postBegin = g_state.postBeginMs > 0.0 ? g_state.postBeginMs : now;
        g_state.sumGameCpuMs += postBegin - g_state.frameBeginMs;
        g_state.sumPostCpuMs += now - postBegin;
        ++g_state.frames;
    }
    if (g_state.frameOpen) {
        g_state.slots[g_state.current].pending = true;
        g_state.frameOpen = false;
    }
    if (now - g_state.windowStartMs >= kReportIntervalMs && g_state.frames > 0) {
        Report(hdc);
    }
    g_state.swapBeginMs = PerfNowMs();
}

void PerfCountDraw() {
    if (Enabled()) {
        ++g_state.draws;
    }
}

void PerfCountUpload(bool throughTextureEffect, double cpuMs) {
    if (!Enabled()) {
        return;
    }
    ++g_state.uploads;
    if (throughTextureEffect) {
        ++g_state.effectUploads;
        g_state.effectUploadMs += cpuMs;
    }
}
