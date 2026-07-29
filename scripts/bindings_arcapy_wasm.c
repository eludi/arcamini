// Browser/WASM variant of bindings_arcapy.c: same pocketpy <-> arcamini API
// surface (identical function names/argument order/defaults to the native
// arcapy runtime), but every binding calls back into the JS engine already
// implemented in browser_runtime/app.js (window.gfx/window.audio/
// window.resource) instead of the native SDL/OpenGL engine (arcamini.c +
// external/arcajs/*, which only exist as prebuilt native static libraries --
// there is no WASM build of them, so they can't be reused here).
//
// Any error -- a Python-level exception (caught by pocketpy itself) or a JS
// exception thrown by the engine call (e.g. window.color(0)) -- is reported
// via js_ReportError, which both surfaces it through app.js's reportError()
// and re-throws in JS. That throw unwinds naturally through the WASM call
// stack back to whichever JS code invoked the WASM export (app.js's pyDriver
// methods), so the existing mainLoop/dispatch try/catch blocks in app.js
// handle a Python failure exactly like a JS-script failure -- no separate
// bool-return plumbing needed per dispatch function.
#include "pocketpy.h"
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// --- error reporting bridge ---
EM_JS(void, js_ReportError, (const char* msg), {
    app._reportError(UTF8ToString(msg));
    throw new Error('arcamini script error');
});

static bool handleException() {
    char* msg = py_formatexc();
    if(msg) {
        js_ReportError(msg); // reports and throws; free(msg)/return below are unreachable in practice
        free(msg);
    }
    return false;
}

// --- window bindings ---
EM_JS(int, js_WindowWidth, (), { return window.width(); });
EM_JS(int, js_WindowHeight, (), { return window.height(); });
EM_JS(void, js_WindowClearColor, (uint32_t color), { window.color(color); });
EM_JS(void, js_WindowSwitchScene, (const char* fname, const char** args, int argc), {
    var arr = [];
    for(var i=0; i<argc; ++i)
        arr.push(UTF8ToString(HEAP32[(args>>2) + i]));
    window.switchScene.apply(null, [UTF8ToString(fname)].concat(arr));
});

static bool py_WindowWidth(int argc, py_StackRef argv) {
    py_newint(py_retval(), js_WindowWidth());
    return true;
}
static bool py_WindowHeight(int argc, py_StackRef argv) {
    py_newint(py_retval(), js_WindowHeight());
    return true;
}
static bool py_WindowClearColor(int argc, py_StackRef argv) {
    int64_t color;
    if(!py_castint(py_arg(0), &color))
        return false;
    js_WindowClearColor((uint32_t)color);
    py_newnone(py_retval());
    return true;
}

// window.switchScene() is asynchronous in the browser (it has to fetch the
// new script), unlike the native runtime where it blocks until the switch
// completes -- it kicks the switch off and returns immediately, same as the
// JS-facing window.switchScene already does in app.js.
static bool py_switchScene(int argc, py_StackRef argv) {
    const char* fname = py_tostr(py_arg(0));
    char** args = argc > 1 ? (char**)malloc((argc - 1) * sizeof(char*)) : NULL;
    for(int i = 1; i < argc; ++i) {
        py_str(py_arg(i));
        args[i - 1] = strdup(py_tostr(py_retval()));
    }
    js_WindowSwitchScene(fname, (const char**)args, argc - 1);
    if(args) {
        for(int i = 0; i < argc - 1; ++i)
            free(args[i]);
        free(args);
    }
    py_newnone(py_retval());
    return true;
}

// --- gfx bindings ---
EM_JS(void, js_gfxColor, (uint32_t color), { window.gfx.color(color); });
EM_JS(void, js_gfxLineWidth, (float w), { window.gfx.lineWidth(w); });
EM_JS(void, js_gfxTransform, (float x, float y, float rot, float sc), { window.gfx.transform(x, y, rot, sc); });
EM_JS(void, js_gfxStateSave, (), { window.gfx.save(); });
EM_JS(void, js_gfxStateRestore, (), { window.gfx.restore(); });
EM_JS(void, js_gfxClipRect, (int x, int y, int w, int h), { window.gfx.clipRect(x, y, w, h); });
EM_JS(void, js_gfxDrawRect, (float x, float y, float w, float h), { window.gfx.drawRect(x, y, w, h); });
EM_JS(void, js_gfxFillRect, (float x, float y, float w, float h), { window.gfx.fillRect(x, y, w, h); });
EM_JS(void, js_gfxDrawLine, (float x0, float y0, float x1, float y1), { window.gfx.drawLine(x0, y0, x1, y1); });
EM_JS(void, js_gfxDrawImage, (uint32_t img, float x, float y, float rot, float sc, int flip), {
    window.gfx.drawImage(img, x, y, rot, sc, flip);
});
EM_JS(void, js_gfxFillTextAlign, (uint32_t font, float x, float y, const char* str, int align), {
    window.gfx.fillText(font, x, y, UTF8ToString(str), align);
});

static bool py_gfxColor(int argc, py_StackRef argv) {
    int64_t color;
    if(!py_castint(py_arg(0), &color))
        return false;
    js_gfxColor((uint32_t)color);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxLineWidth(int argc, py_StackRef argv) {
    float w;
    if(!py_castfloat32(py_arg(0), &w))
        return false;
    js_gfxLineWidth(w);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxTransform(int argc, py_StackRef argv) {
    float x, y, rot=0.0f, sc=1.0f;
    if(!py_castfloat32(py_arg(0), &x) ||
       !py_castfloat32(py_arg(1), &y))
       return false;
    if(argc > 2 && !py_castfloat32(py_arg(2), &rot))
       return false;
    if(argc > 3 && !py_castfloat32(py_arg(3), &sc))
        return false;
    js_gfxTransform(x, y, rot, sc);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxStateSave(int argc, py_StackRef argv) {
    (void)argc; (void)argv;
    js_gfxStateSave();
    py_newnone(py_retval());
    return true;
}

static bool py_gfxStateRestore(int argc, py_StackRef argv) {
    (void)argc; (void)argv;
    js_gfxStateRestore();
    py_newnone(py_retval());
    return true;
}

static bool py_gfxClipRect(int argc, py_StackRef argv) {
    int64_t x, y, w, h;
    if(!py_castint(py_arg(0), &x) ||
       !py_castint(py_arg(1), &y) ||
       !py_castint(py_arg(2), &w) ||
       !py_castint(py_arg(3), &h))
        return false;
    js_gfxClipRect((int)x, (int)y, (int)w, (int)h);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxDrawRect(int argc, py_StackRef argv) {
    float x, y, w, h;
    if(!py_castfloat32(py_arg(0), &x) ||
       !py_castfloat32(py_arg(1), &y) ||
       !py_castfloat32(py_arg(2), &w) ||
       !py_castfloat32(py_arg(3), &h))
        return false;
    js_gfxDrawRect(x, y, w, h);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxFillRect(int argc, py_StackRef argv) {
    float x, y, w, h;
    if(!py_castfloat32(py_arg(0), &x) ||
       !py_castfloat32(py_arg(1), &y) ||
       !py_castfloat32(py_arg(2), &w) ||
       !py_castfloat32(py_arg(3), &h))
        return false;
    js_gfxFillRect(x, y, w, h);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxDrawLine(int argc, py_StackRef argv) {
    float x0, y0, x1, y1;
    if(!py_castfloat32(py_arg(0), &x0) ||
       !py_castfloat32(py_arg(1), &y0) ||
       !py_castfloat32(py_arg(2), &x1) ||
       !py_castfloat32(py_arg(3), &y1))
        return false;
    js_gfxDrawLine(x0, y0, x1, y1);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxDrawImage(int argc, py_StackRef argv) {
    int64_t img, flip = 0;
    float x, y, rot = 0.0f, sc = 1.0f;
    if(!py_castint(py_arg(0), &img) ||
       !py_castfloat32(py_arg(1), &x) ||
       !py_castfloat32(py_arg(2), &y))
        return false;
    if(argc > 3 && !py_castfloat32(py_arg(3), &rot))
        return false;
    if(argc > 4 && !py_castfloat32(py_arg(4), &sc))
        return false;
    if(argc > 5 && !py_castint(py_arg(5), &flip))
        return false;
    js_gfxDrawImage((uint32_t)img, x, y, rot, sc, (int)flip);
    py_newnone(py_retval());
    return true;
}

static bool py_gfxFillTextAlign(int argc, py_StackRef argv) {
    int64_t font, align = 0;
    float x, y;
    py_str(py_arg(3));
    const char* str = py_tostr(py_retval());
    if(!py_castint(py_arg(0), &font) ||
       !py_castfloat32(py_arg(1), &x) ||
       !py_castfloat32(py_arg(2), &y))
        return false;
    if(argc > 4 && !py_castint(py_arg(4), &align))
        return false;
    js_gfxFillTextAlign((uint32_t)font, x, y, str, (int)align);
    py_newnone(py_retval());
    return true;
}

// --- audio bindings ---
EM_JS(int, js_AudioReplay, (uint32_t sample, float vol, float bal, float det), {
    var track = window.audio.replay(sample, vol, bal, det);
    return (track === undefined) ? -1 : track;
});
EM_JS(void, js_AudioVolume, (uint32_t track, float volume, float fadeTime), {
    window.audio.volume(track, volume, fadeTime);
});

static bool py_AudioReplay(int argc, py_StackRef argv) {
    int64_t sample;
    float vol=1.0f, bal=0.0f, det=0.0f;
    if(!py_castint(py_arg(0), &sample))
        return false;
    if(argc > 1 && !py_castfloat32(py_arg(1), &vol))
        return false;
    if(argc > 2 && !py_castfloat32(py_arg(2), &bal))
        return false;
    if(argc > 3 && !py_castfloat32(py_arg(3), &det))
        return false;

    int track = js_AudioReplay((uint32_t)sample, vol, bal, det);
    if(track < 0)
        py_newnone(py_retval());
    else
        py_newint(py_retval(), track);
    return true;
}

static bool py_AudioVolume(int argc, py_StackRef argv) {
    int64_t track;
    float volume, fadeTime = 0.0f;
    if(!py_castint(py_arg(0), &track) ||
       !py_castfloat32(py_arg(1), &volume))
        return false;
    if(argc > 2 && !py_castfloat32(py_arg(2), &fadeTime))
        return false;
    js_AudioVolume((uint32_t)track, volume, fadeTime);
    py_newnone(py_retval());
    return true;
}

// --- resource bindings ---
EM_JS(int, js_ResourceGetImage, (const char* name, float scale, float cx, float cy, int filtering), {
    return window.resource.getImage(UTF8ToString(name), scale, cx, cy, filtering);
});
EM_JS(int, js_ResourceCreateImage, (const uint8_t* data, int len, int width, int height, float cx, float cy, int filtering), {
    var bytes = HEAPU8.slice(data, data + len);
    return window.resource.createImage(new Uint32Array(bytes.buffer), width, height, cx, cy, filtering);
});
EM_JS(int, js_ResourceCreateSVGImage, (const char* svg, float scale, float cx, float cy), {
    return window.resource.createSVGImage(UTF8ToString(svg), scale, cx, cy);
});
EM_JS(int, js_ResourceGetTileImage, (uint32_t parent, int x, int y, int w, int h, float cx, float cy), {
    return window.resource.getTileImage(parent, x, y, w, h, cx, cy);
});
EM_JS(int, js_ResourceGetTileGrid, (uint32_t img, int tilesX, int tilesY, int borderW), {
    return window.resource.getTileGrid(img, tilesX, tilesY, borderW);
});
EM_JS(int, js_ResourceGetAudio, (const char* name), {
    return window.resource.getAudio(UTF8ToString(name));
});
EM_JS(int, js_ResourceCreateAudio, (const float* data, int numSamples, int numChannels), {
    var bytes = HEAPU8.slice(data, data + numSamples * 4);
    return window.resource.createAudio(new Float32Array(bytes.buffer), numChannels);
});
EM_JS(int, js_ResourceGetFont, (const char* name, uint32_t fontSize), {
    return window.resource.getFont(UTF8ToString(name), fontSize);
});
// These three wrap their JS call in try/catch and return a sentinel (0, or
// NaN for the float-returning queryFont) instead of letting the underlying
// exception (window.resource.query*() throws on an invalid handle/property)
// propagate raw: a JS exception thrown inside an EM_JS body unwinds straight
// through the WASM/C call stack -- including pocketpy's own interpreter loop
// frames -- back out to the JS caller of Module.ccall(), completely
// bypassing pocketpy's try/except mechanism (found via arcamini_test.py's
// own try/except around a deliberate invalid-handle call not catching
// anything). The C wrappers below convert the sentinel into a proper
// pocketpy exception instead, exactly matching native bindings_arcapy.c.
EM_JS(int, js_ResourceQueryImage, (uint32_t image, const char* property), {
    try { return window.resource.queryImage(image, UTF8ToString(property)); }
    catch(e) { return 0; }
});
EM_JS(int, js_ResourceQueryAudio, (uint32_t sample, const char* property), {
    try { return window.resource.queryAudio(sample, UTF8ToString(property)); }
    catch(e) { return 0; }
});
EM_JS(double, js_ResourceQueryFont, (uint32_t font, const char* property, const char* str), {
    try { return window.resource.queryFont(font, UTF8ToString(property), UTF8ToString(str)); }
    catch(e) { return NaN; }
});
EM_JS(char*, js_ResourceGetStorageItem, (const char* key), {
    var val = window.resource.getStorageItem(UTF8ToString(key));
    return (val === null) ? 0 : allocateUTF8(val);
});
EM_JS(void, js_ResourceSetStorageItem, (const char* key, const char* value), {
    window.resource.setStorageItem(UTF8ToString(key), UTF8ToString(value));
});

static bool py_ResourceGetImage(int argc, py_StackRef argv) {
    const char* name = py_tostr(py_arg(0));
    float scale = 1.0f, centerX=0.0f, centerY=0.0f;
    int64_t filtering = 1;
    if(argc > 1 && !py_castfloat32(py_arg(1), &scale))
        return false;
    if(argc > 2 && !py_castfloat32(py_arg(2), &centerX))
        return false;
    if(argc > 3 && !py_castfloat32(py_arg(3), &centerY))
        return false;
    if(argc > 4 && !py_castint(py_arg(4), &filtering))
        return false;

    int handle = js_ResourceGetImage(name, scale, centerX, centerY, (int)filtering);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

// binding for resource.createImage(array, width, height, centerX, centerY, filtering)
static bool py_ResourceCreateImage(int argc, py_StackRef argv) {
    // read data from an array/list containing uint32_t color values:
    const size_t numItems = py_islist(py_arg(0)) ? py_list_len(py_arg(0)) : 0;
    if(!numItems)
        return TypeError("resource.createImage() expects non-empty list containing numeric color values as first argument");

    size_t numBytes = numItems * sizeof(uint32_t);
    uint32_t* data = malloc(numBytes);
    int64_t color;
    for(size_t i=0; i<numItems; ++i) {
        py_ItemRef item = py_list_getitem(py_arg(0), i);
        if(!py_castint(item, &color) || color<0 || color>0xFFFFFFFF) {
            free(data);
            return ValueError("resource.createImage() argument 0 expects numeric color value at position %i\n", (int64_t)i);
        }
        data[i] = (uint32_t)color;
    }

    int64_t width, height, filtering = 1;
    float centerX = 0.0f, centerY = 0.0f;
    if(!py_castint(py_arg(1), &width) ||
       !py_castint(py_arg(2), &height)) {
        free(data);
        return false;
    }
    if(width*height < numItems) {
        free(data);
        return ValueError("resource.createImage() argument 1 expects width*height >= %zu\n", numItems);
    }
    if(argc > 3 && !py_castfloat32(py_arg(3), &centerX)) {
        free(data);
        return false;
    }
    if(argc > 4 && !py_castfloat32(py_arg(4), &centerY)) {
        free(data);
        return false;
    }
    if(argc > 5 && !py_castint(py_arg(5), &filtering)) {
        free(data);
        return false;
    }

    int handle = js_ResourceCreateImage((const uint8_t*)data, (int)numBytes, (int)width, (int)height, centerX, centerY, (int)filtering);
    free(data);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

static bool py_ResourceCreateSVGImage(int argc, py_StackRef argv) {
    const char* svg = py_tostr(py_arg(0));
    float scale = 1.0f, centerX=0.0f, centerY=0.0f;
    if(argc > 1 && !py_castfloat32(py_arg(1), &scale))
        return false;
    if(argc > 2 && !py_castfloat32(py_arg(2), &centerX))
        return false;
    if(argc > 3 && !py_castfloat32(py_arg(3), &centerY))
        return false;

    int handle = js_ResourceCreateSVGImage(svg, scale, centerX, centerY);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

static bool py_ResourceGetTileImage(int argc, py_StackRef argv) {
    int64_t parent, x, y, width, height;
    float centerX=0.0f, centerY=0.0f;
    if(!py_castint(py_arg(0), &parent) ||
       !py_castint(py_arg(1), &x) ||
       !py_castint(py_arg(2), &y) ||
       !py_castint(py_arg(3), &width) ||
       !py_castint(py_arg(4), &height))
        return false;
    if(argc > 5 && !py_castfloat32(py_arg(5), &centerX))
        return false;
    if(argc > 6 && !py_castfloat32(py_arg(6), &centerY))
        return false;
    int handle = js_ResourceGetTileImage(
        (uint32_t)parent, (int)x, (int)y, (int)width, (int)height, centerX, centerY);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

static bool py_ResourceGetTileGrid(int argc, py_StackRef argv) {
    int64_t img, tilesX, tilesY = 1, borderW = 0;
    if(!py_castint(py_arg(0), &img) || !py_castint(py_arg(1), &tilesX))
        return false;
    if(argc > 2 && !py_castint(py_arg(2), &tilesY))
        return false;
    if(argc > 3 && !py_castint(py_arg(3), &borderW))
        return false;

    int handle = js_ResourceGetTileGrid((uint32_t)img, (int)tilesX, (int)tilesY, (int)borderW);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

static bool py_ResourceGetAudio(int argc, py_StackRef argv) {
    const char* name = py_tostr(py_arg(0));
    int handle = js_ResourceGetAudio(name);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

// binding for resource.createAudio(list, numChannels)
static bool py_ResourceCreateAudio(int argc, py_StackRef argv) {
    // read data from an array/list containing float sample values:
    const size_t numSamples = py_islist(py_arg(0)) ? py_list_len(py_arg(0)) : 0;
    if(!numSamples)
        return TypeError("resource.createAudio() expects non-empty list containing numeric sample values as first argument");

    float* data = malloc(numSamples * sizeof(float));
    for(size_t i=0; i<numSamples; ++i) {
        py_ItemRef item = py_list_getitem(py_arg(0), i);
        if(!py_castfloat32(item, &data[i]) || data[i] < -1.0f || data[i] > 1.0f) {
            free(data);
            return ValueError("resource.createAudio() argument 0 expects numeric sample value between -1.0 and 1.0 at position %i\n", (int64_t)i);
        }
    }

    int64_t numChannels = 1;
    if(argc > 1 && !py_castint(py_arg(1), &numChannels)) {
        free(data);
        return false;
    }

    int handle = js_ResourceCreateAudio(data, (int)numSamples, (int)numChannels);
    free(data);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

static bool py_ResourceGetFont(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    const char* name = py_tostr(py_arg(0));
    int64_t fontSize;
    if(!py_castint(py_arg(1), &fontSize))
        return false;

    int handle = js_ResourceGetFont(name, (uint32_t)fontSize);
    py_newint(py_retval(), (int64_t)handle);
    return true;
}

static bool py_ResourceQueryImage(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    int64_t image;
    if(!py_castint(py_arg(0), &image))
        return false;
    const char* property = py_tostr(py_arg(1));
    int value = js_ResourceQueryImage((uint32_t)image, property);
    if(!value)
        return ValueError("resource.queryImage(%i, '%s') failed: invalid image handle or unrecognized property\n", image, property);
    py_newint(py_retval(), (int64_t)value);
    return true;
}

static bool py_ResourceQueryAudio(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    int64_t sample;
    if(!py_castint(py_arg(0), &sample))
        return false;
    const char* property = py_tostr(py_arg(1));
    int value = js_ResourceQueryAudio((uint32_t)sample, property);
    if(!value)
        return ValueError("resource.queryAudio(%i, '%s') failed: invalid audio handle or unrecognized property\n", sample, property);
    py_newint(py_retval(), (int64_t)value);
    return true;
}

static bool py_ResourceQueryFont(int argc, py_StackRef argv) {
    int64_t font;
    if(!py_castint(py_arg(0), &font))
        return false;
    const char* property = py_tostr(py_arg(1));
    const char* str = argc > 2 ? py_tostr(py_arg(2)) : "M";
    double value = js_ResourceQueryFont((uint32_t)font, property, str);
    if(isnan(value))
        return ValueError("resource.queryFont(%i, '%s') failed: invalid font handle or unrecognized property\n", font, property);
    py_newfloat(py_retval(), value);
    return true;
}

static bool py_ResourceGetStorageItem(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(1);
    const char* key = py_tostr(py_arg(0));
    char* val = js_ResourceGetStorageItem(key);
    if(!val) py_newnone(py_retval());
    else {
        py_newstr(py_retval(), val);
        free(val);
    }
    return true;
}

static bool py_ResourceSetStorageItem(int argc, py_StackRef argv) {
    PY_CHECK_ARGC(2);
    const char* key = py_tostr(py_arg(0));
    py_str(py_arg(1));
    const char* val = py_tostr(py_retval());
    js_ResourceSetStorageItem(key, val);
    py_newnone(py_retval());
    return true;
}

/// --- Bindings ---
static py_GlobalRef gfx_ns = NULL;

static void bindArcamini() {
    // unified arcamini namespace
    py_GlobalRef arcamini_ns = py_newmodule("arcamini");

    py_Ref window_ns = py_newmodule("window");
    py_bindfunc(window_ns, "width", py_WindowWidth);
    py_bindfunc(window_ns, "height", py_WindowHeight);
    py_bindfunc(window_ns, "color", py_WindowClearColor);
    py_bindfunc(window_ns, "switchScene", py_switchScene);
    py_setdict(arcamini_ns, py_name("window"), window_ns);

    // gfx namespace, only used by draw callback
    gfx_ns = py_newmodule("gfx");
    py_bindfunc(gfx_ns, "color", py_gfxColor);
    py_bindfunc(gfx_ns, "lineWidth", py_gfxLineWidth);
    py_bindfunc(gfx_ns, "transform", py_gfxTransform);
    py_bindfunc(gfx_ns, "save", py_gfxStateSave);
    py_bindfunc(gfx_ns, "restore", py_gfxStateRestore);
    py_bindfunc(gfx_ns, "clipRect", py_gfxClipRect);
    py_bindfunc(gfx_ns, "drawRect", py_gfxDrawRect);
    py_bindfunc(gfx_ns, "fillRect", py_gfxFillRect);
    py_bindfunc(gfx_ns, "drawLine", py_gfxDrawLine);
    py_bindfunc(gfx_ns, "drawImage", py_gfxDrawImage);
    py_bindfunc(gfx_ns, "fillText", py_gfxFillTextAlign);

    // audio namespace
    py_Ref audio_ns = py_newmodule("audio");
    py_bindfunc(audio_ns, "replay", py_AudioReplay);
    py_bindfunc(audio_ns, "volume", py_AudioVolume);
    py_setdict(arcamini_ns, py_name("audio"), audio_ns);

    // resource namespace
    py_Ref resource_ns = py_newmodule("resource");
    py_bindfunc(resource_ns, "getImage", py_ResourceGetImage);
    py_bindfunc(resource_ns, "createImage", py_ResourceCreateImage);
    py_bindfunc(resource_ns, "createSVGImage", py_ResourceCreateSVGImage);
    py_bindfunc(resource_ns, "getTileImage", py_ResourceGetTileImage);
    py_bindfunc(resource_ns, "getTileGrid", py_ResourceGetTileGrid);
    py_bindfunc(resource_ns, "getAudio", py_ResourceGetAudio);
    py_bindfunc(resource_ns, "createAudio", py_ResourceCreateAudio);
    py_bindfunc(resource_ns, "getFont", py_ResourceGetFont);
    py_bindfunc(resource_ns, "queryImage", py_ResourceQueryImage);
    py_bindfunc(resource_ns, "queryAudio", py_ResourceQueryAudio);
    py_bindfunc(resource_ns, "queryFont", py_ResourceQueryFont);
    py_bindfunc(resource_ns, "getStorageItem", py_ResourceGetStorageItem);
    py_bindfunc(resource_ns, "setStorageItem", py_ResourceSetStorageItem);
    py_setdict(arcamini_ns, py_name("resource"), resource_ns);
}

/// --- VM Management ---

// Loads a second .py file the game imports itself (import my_helper), via a
// synchronous XHR -- matching the native runtime's blocking
// ResourceGetText(module_name) read exactly. This briefly blocks the page
// for that one fetch (deprecated API, but functional everywhere); it's the
// only way to satisfy Python's synchronous import semantics without
// restricting games to single-file scripts. Returns a malloc'd buffer (via
// Module.allocateUTF8, which uses the same heap as libc malloc/free) that
// pocketpy takes ownership of, matching the native custom_importfile
// contract; NULL (module not found) lets pocketpy raise its own clear
// ImportError.
EM_JS(char*, js_ImportFile, (const char* moduleName), {
    // pocketpy already passes the fully-suffixed candidate name itself
    // (e.g. "helper.py", "helper.pyc", "helper/__init__.py", ...) -- fetch
    // it as-is, matching native's ResourceGetText(module_name).
    var xhr = new XMLHttpRequest();
    xhr.open('GET', UTF8ToString(moduleName), false);
    xhr.send(null);
    if(xhr.status !== 200)
        return 0;
    return allocateUTF8(xhr.responseText);
});

static char* custom_importfile(const char* module_name, int* data_size) {
    char* script = js_ImportFile(module_name);
    if(!script)
        fprintf(stderr, "Module not found: %s\n", module_name);
    else if(data_size)
        *data_size = (int)strlen(script);
    return script;
}

EMSCRIPTEN_KEEPALIVE
void shutdownVM(void* context) {
    if(context)
        py_finalize();
}

// Creates and initializes a PocketPy VM. Returns a non-NULL sentinel handle
// on success (pocketpy itself has no notion of multiple VM instances), or
// NULL if the script failed to evaluate.
EMSCRIPTEN_KEEPALIVE
void* initVM(const char* script, const char* scriptName) {
    py_initialize();
    py_callbacks()->importfile = custom_importfile;
    bindArcamini();
    void* ctx = (void*)1;

    bool ok = py_exec(script, scriptName, EXEC_MODE, NULL);
    if(!ok || py_checkexc()) {
        py_printexc();
        shutdownVM(ctx);
        return NULL;
    }
    return ctx;
}

/// --- Event dispatchers ---
// These only touch the pocketpy C API (no engine calls), so they're carried
// over from bindings_arcapy.c almost unchanged.

EMSCRIPTEN_KEEPALIVE
bool dispatchLifecycleEvent(const char* evtName, void* callback) {
    (void)callback;
    py_Ref fn = py_getglobal(py_name(evtName));
    if(!fn || (py_typeof(fn) != tp_function && py_typeof(fn) != tp_nativefunc))
        return true;

    py_push(fn);
    py_pushnil();
    if(!py_vectorcall(0, 0))
        return handleException();
    return true;
}

EMSCRIPTEN_KEEPALIVE
bool dispatchLifecycleEventArgv(const char* evtName, int argc, char** argv, void* callback) {
    (void)callback;
    py_Ref fn = py_getglobal(py_name(evtName));
    if(!fn || (py_typeof(fn) != tp_function && py_typeof(fn) != tp_nativefunc))
        return true;

    py_push(fn);
    py_pushnil();
    // dispatch arguments as a single list:
    py_newlist(py_getreg(0));
    for(int i=0; i<argc; ++i) {
        py_Ref val = py_getreg(1);
        py_newstr(val, argv[i]);
        py_list_append(py_getreg(0), val);
    }
    py_push(py_getreg(0));
    if(!py_vectorcall(1, 0))
        return handleException();
    return true;
}

EMSCRIPTEN_KEEPALIVE
void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* callback) {
    (void)callback;
    py_Ref fnInput = py_getglobal(py_name("input"));
    if(!fnInput || (py_typeof(fnInput) != tp_function && py_typeof(fnInput) != tp_nativefunc))
        return;

    py_push(fnInput);
    py_pushnil();
    py_Ref val = py_getreg(0);

    py_newstr(val, "axis");
    py_push(val);
    py_newint(val, id);
    py_push(val);
    py_newint(val, axis);
    py_push(val);
    py_newfloat(val, value);
    py_push(val);
    py_pushnone();

    if(!py_vectorcall(5, 0))
        handleException();
}

EMSCRIPTEN_KEEPALIVE
void dispatchButtonEvent(size_t id, uint8_t button, float value, void* callback) {
    // close-on-buttons-6+7 is handled once, uniformly across languages, by
    // app.js's checkCloseButtons() before this is even called -- no need to
    // duplicate that bookkeeping here.
    (void)callback;
    py_Ref fnInput = py_getglobal(py_name("input"));
    if(!fnInput || (py_typeof(fnInput) != tp_function && py_typeof(fnInput) != tp_nativefunc))
        return;

    py_push(fnInput);
    py_pushnil();
    py_Ref val = py_getreg(0);

    py_newstr(val, "button");
    py_push(val);
    py_newint(val, id);
    py_push(val);
    py_newint(val, button);
    py_push(val);
    py_newfloat(val, value);
    py_push(val);
    py_pushnone();

    if(!py_vectorcall(5, 0))
        handleException();
}

EMSCRIPTEN_KEEPALIVE
bool dispatchUpdateEvent(double deltaT, void* callback) {
    (void)callback;
    py_Ref fnUpdate = py_getglobal(py_name("update"));
    if(!fnUpdate || (py_typeof(fnUpdate) != tp_function && py_typeof(fnUpdate) != tp_nativefunc))
        return false;
    py_push(fnUpdate);
    py_pushnil();
    py_Ref arg0 = py_getreg(0);
    py_newfloat(arg0, deltaT);
    py_push(arg0);
    if(!py_vectorcall(1, 0))
        return handleException();
    return py_bool(py_retval()) > 0;
}

// Deviates from bindings.h's void signature: the browser main loop needs to
// know whether draw() failed (to halt, matching update()'s contract), which
// native doesn't need since a native draw() exception halts via
// WindowEmitClose() + a blocking full-screen error display instead.
EMSCRIPTEN_KEEPALIVE
bool dispatchDrawEvent(void* callback) {
    (void)callback;
    py_Ref fnDraw = py_getglobal(py_name("draw"));
    if(!fnDraw || (py_typeof(fnDraw) != tp_function && py_typeof(fnDraw) != tp_nativefunc))
        return true;

    py_push(fnDraw);
    py_pushnil();
    py_push(gfx_ns);
    if(!py_vectorcall(1, 0))
        return handleException();
    return true;
}
