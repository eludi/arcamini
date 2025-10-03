#include "window.h"
#include "graphics.h"
#include "audio.h"
#include "resources.h"
#include "arcamini.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

/// opcodes for batched graphics calls
enum {
    GFX_OP_COLOR = 1,
    GFX_OP_LINEWIDTH,
    GFX_OP_FILLRECT,
    GFX_OP_DRAWRECT,
    GFX_OP_DRAWLINE,
    GFX_OP_DRAWIMAGE,
    GFX_OP_FILLTEXT,
};

void gfxDrawBatch(const uint8_t* ops, uint32_t ops_len, const char* strings, uint32_t strings_len) {
    const uint8_t* p = ops;
    const uint8_t* end = p + ops_len;
    uint32_t opcode;
    while (p < end) {
        memcpy(&opcode, p, 4); p += sizeof(opcode);
        switch (opcode) {
            case GFX_OP_COLOR: {
                uint32_t clr;
                memcpy(&clr, p, 4); p += sizeof(clr);
                gfxColor(clr);
            } break;
            case GFX_OP_LINEWIDTH: {
                float w;
                memcpy(&w, p, 4); p += sizeof(w);
                gfxLineWidth(w);
            } break;
            case GFX_OP_FILLRECT: 
            case GFX_OP_DRAWRECT: {
                float x, y, w, h;
                memcpy(&x, p, 4); p += sizeof(x);
                memcpy(&y, p, 4); p += sizeof(y);
                memcpy(&w, p, 4); p += sizeof(w);
                memcpy(&h, p, 4); p += sizeof(h);
                if(opcode == GFX_OP_FILLRECT)
                    gfxFillRect(x, y, w, h);
                else
                    gfxDrawRect(x, y, w, h);
            } break;
            case GFX_OP_DRAWLINE: {
                float x1, y1, x2, y2;
                memcpy(&x1, p, 4); p += sizeof(x1);
                memcpy(&y1, p, 4); p += sizeof(y1);
                memcpy(&x2, p, 4); p += sizeof(x2);
                memcpy(&y2, p, 4); p += sizeof(y2);
                gfxDrawLine(x1, y1, x2, y2);
            } break;
            case GFX_OP_DRAWIMAGE: {
                uint32_t img;
                float x, y, rot, sc;
                int flip;
                memcpy(&img, p, 4); p += sizeof(img);
                memcpy(&x, p, 4); p += sizeof(x);
                memcpy(&y, p, 4); p += sizeof(y);
                memcpy(&rot, p, 4); p += sizeof(rot);
                memcpy(&sc, p, 4); p += sizeof(sc);
                memcpy(&flip, p, 4); p += sizeof(flip);
                gfxDrawImage(img, x, y, rot, sc, flip);
            } break;
            case GFX_OP_FILLTEXT: {
                uint32_t font, textOffset, align;
                float x, y;
                memcpy(&font, p, 4); p += sizeof(font);
                memcpy(&x, p, 4); p += sizeof(x);
                memcpy(&y, p, 4); p += sizeof(y);
                memcpy(&textOffset, p, 4); p += sizeof(textOffset);
                memcpy(&align, p, 4); p += sizeof(align);
                if(textOffset >= strings_len) {
                    fprintf(stderr, "gfxRenderBatch: textOffset %u out of bounds (strings_len=%u)\n", textOffset, strings_len);
                    return;
                }
                gfxFillTextAlign(font, x, y, strings + textOffset, align);
            } break;
            default:
                fprintf(stderr, "gfxRenderBatch: unknown opcode %u at position %ld\n", opcode, p - ops);
                return;
        }
    }
}

// Callback types
typedef bool (*am_update_cb_t)(double dt);
typedef void (*am_draw_cb_t)();
typedef void (*am_input_cb_t)(const char* evt, int device, int id, float value, float value2);

// Globals for callbacks
static am_update_cb_t g_update = NULL;
static am_draw_cb_t   g_draw   = NULL;
static am_input_cb_t  g_input  = NULL;

// global variables
bool isRunning = true;
int debug = 0;

bool arcamini_init(int winSzX, int winSzY, bool fullscreen, const char* archiveName, const char* scriptBaseName) {
    if(debug)
        printf("arcamini_init(width=%d, height=%d, fullscreen=%s, archiveName=%s)\n", winSzX, winSzY, fullscreen ? "true" : "false", archiveName);

    if(!ResourceArchiveOpen(archiveName)) {
        fprintf(stderr, "Opening archive \"%s\" failed.\n", archiveName);
        return false;
    }

    srand(time(NULL));
    AudioOpen(44100, 8);

    int windowFlags = WINDOW_VSYNC;
    if (fullscreen)
        windowFlags |= WINDOW_FULLSCREEN;
    if(WindowOpen(winSzX, winSzY, windowFlags)!=0) {
        fprintf(stderr, "Setting video mode failed.\n");
        return false;
    }
    winSzX = WindowWidth();
    winSzY = WindowHeight();
    gfxInit(winSzX, winSzY, 1, WindowRenderer());
    WindowTitle(scriptBaseName);
    WindowShowPointer(0);

    arcmStorageInit("arcapy", scriptBaseName);

    for(size_t i=0; i<WindowNumControllers(); ++i)
        WindowControllerOpen(i, 0);
    WindowEventHandler(arcmDispatchInputEvents, NULL);
    return true;
}

void arcamini_shutdown() {
    if(debug) {
        printf("Shutting down... "); fflush(stdout);
    }
    arcmStorageClose();
    gfxClose();
    if(WindowIsOpen())
        WindowClose();
    AudioClose();
    ResourceArchiveClose();
    if(debug)
        printf("done.\n");
}

void arcamini_set_callbacks(am_input_cb_t input_cb, am_update_cb_t update_cb, am_draw_cb_t draw_cb) {
    g_input = input_cb;
    g_update = update_cb;
    g_draw = draw_cb;
}

// main loop
void arcamini_run(void) {

    if (!g_update || !g_draw || !g_input) {
        fprintf(stderr, "ERROR: callbacks not set!\n");
        return;
    }

    isRunning = true;
    while (isRunning && WindowIsOpen()) {
        if (!g_update(WindowDeltaT()))
            break;

        gfxBeginFrame(WindowGetClearColor());
        g_draw();
        gfxEndFrame();

        if(WindowUpdate()!=0)
            break;
    }
    arcamini_shutdown();
}

uint32_t arcamini_createAudio(float* waveData, uint32_t numSamples, uint8_t numChannels) {
    float* waveDataCopy = malloc(numSamples * numChannels * sizeof(float));
    if(!waveDataCopy) {
        fprintf(stderr, "arcamini_createAudio: out of memory\n");
        return 0;
    }
    memcpy(waveDataCopy, waveData, numSamples * numChannels * sizeof(float));
    return AudioUploadPCM(waveDataCopy, numSamples, numChannels, 0);
}


void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* callback) {
	(void)callback;
    if (g_input)
        g_input("axis", id, axis, value, 0.0f);
}

void dispatchButtonEvent(size_t id, uint8_t button, float value, void* callback) {
	(void)callback;
	arcmWindowCloseOnButton67(id, button, value);
    if (g_input)
        g_input("button", id, button, value, 0.0f);
}
