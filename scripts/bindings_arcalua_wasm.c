// Browser/WASM variant of bindings_arcalua.c: same Lua <-> arcamini API
// surface (identical function names/argument order/defaults to the native
// arcalua runtime), but every binding calls back into the JS engine already
// implemented in browser_runtime/app.js (window.gfx/window.audio/
// window.resource) instead of the native SDL/OpenGL engine (arcamini.c +
// external/arcajs/*, which only exist as prebuilt native static libraries --
// there is no WASM build of them, so they can't be reused here).
//
// Any error -- a Lua-level error (caught by lua_pcall) or a JS exception
// thrown by an engine call (e.g. window.color(0)) -- is reported via
// js_ReportError, which both surfaces it through app.js's reportError() and
// re-throws in JS. That throw unwinds naturally through the WASM call stack
// back to whichever JS code invoked the WASM export (app.js's luaDriver
// methods), so the existing mainLoop/dispatch try/catch blocks in app.js
// handle a Lua failure exactly like a JS-script failure.
#include <emscripten.h>
#define LUA_IMPL
#include "minilua.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// --- error reporting bridge ---
EM_JS(void, js_ReportError, (const char* msg), {
    app._reportError(UTF8ToString(msg));
    throw new Error('arcamini script error');
});

static void handleException(lua_State *L) {
    const char* msg = lua_tostring(L, -1);
    if(!msg)
        msg = "Lua exception (error object is not a string)";
    fprintf(stderr, "--- ERROR ---\n%s\n", msg);
    js_ReportError(msg); // reports and throws; lua_pop below is unreachable in practice
    lua_pop(L, 1);
}

// --- Window Functions ---
EM_JS(int, js_WindowWidth, (), { return window.width(); });
EM_JS(int, js_WindowHeight, (), { return window.height(); });
EM_JS(void, js_WindowClearColor, (uint32_t color), { window.color(color); });
EM_JS(void, js_WindowSwitchScene, (const char* fname, const char** args, int argc), {
    var arr = [];
    for(var i=0; i<argc; ++i)
        arr.push(UTF8ToString(HEAP32[(args>>2) + i]));
    window.switchScene.apply(null, [UTF8ToString(fname)].concat(arr));
});

static int lua_WindowWidth(lua_State *L) {
    lua_pushinteger(L, js_WindowWidth());
    return 1;
}

static int lua_WindowHeight(lua_State *L) {
    lua_pushinteger(L, js_WindowHeight());
    return 1;
}

static int lua_WindowClearColor(lua_State *L) {
    uint32_t color = (uint32_t)luaL_checkinteger(L, 1);
    js_WindowClearColor(color);
    return 0;
}

// window.switchScene() is asynchronous in the browser (it has to fetch the
// new script), unlike the native runtime where it blocks until the switch
// completes -- it kicks the switch off and returns immediately, same as the
// JS-facing window.switchScene already does in app.js.
static int lua_WindowSwitchScene(lua_State *L) {
    const char* fname = luaL_checkstring(L, 1);
    int argc = lua_gettop(L);
    char** args = argc > 1 ? (char**)malloc((argc - 1) * sizeof(char*)) : NULL;
    for(int i = 1; i < argc; ++i)
        args[i - 1] = strdup(lua_tostring(L, i + 1));

    js_WindowSwitchScene(fname, (const char**)args, argc - 1);
    if(args) {
        for(int i = 0; i < argc - 1; ++i)
            free(args[i]);
        free(args);
    }
    return 0;
}

static const luaL_Reg window_funcs[] = {
    {"width", lua_WindowWidth},
    {"height", lua_WindowHeight},
    {"color", lua_WindowClearColor},
    {"switchScene", lua_WindowSwitchScene},
    {NULL, NULL}
};

// --- Graphics Functions ---
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

static int lua_gfxColor(lua_State *L) {
    uint32_t color = (uint32_t)luaL_checkinteger(L, 1);
    js_gfxColor(color);
    return 0;
}

static int lua_gfxLineWidth(lua_State *L) {
    float width = (float)luaL_checknumber(L, 1);
    js_gfxLineWidth(width);
    return 0;
}

static int lua_gfxTransform(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float rot = (float)luaL_optnumber(L, 3, 0.0f);
    float sc = (float)luaL_optnumber(L, 4, 1.0f);
    js_gfxTransform(x, y, rot, sc);
    return 0;
}

static int lua_gfxStateSave(lua_State *L) {
    (void)L;
    js_gfxStateSave();
    return 0;
}

static int lua_gfxStateRestore(lua_State *L) {
    (void)L;
    js_gfxStateRestore();
    return 0;
}

static int lua_gfxClipRect(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    js_gfxClipRect(x, y, w, h);
    return 0;
}

static int lua_gfxDrawRect(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    js_gfxDrawRect(x, y, w, h);
    return 0;
}

static int lua_gfxFillRect(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    js_gfxFillRect(x, y, w, h);
    return 0;
}

static int lua_gfxDrawLine(lua_State *L) {
    float x0 = (float)luaL_checknumber(L, 1);
    float y0 = (float)luaL_checknumber(L, 2);
    float x1 = (float)luaL_checknumber(L, 3);
    float y1 = (float)luaL_checknumber(L, 4);
    js_gfxDrawLine(x0, y0, x1, y1);
    return 0;
}

static int lua_gfxDrawImage(lua_State *L) {
    uint32_t img = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float rot = (float)luaL_optnumber(L, 4, 0.0f);
    float sc = (float)luaL_optnumber(L, 5, 1.0f);
    int flip = (int)luaL_optnumber(L, 6, 0);
    js_gfxDrawImage(img, x, y, rot, sc, flip);
    return 0;
}

static int lua_gfxFillTextAlign(lua_State *L) {
    uint32_t font = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    const char* str = luaL_checkstring(L, 4);
    int align = (int)luaL_optinteger(L, 5, 0);
    js_gfxFillTextAlign(font, x, y, str, align);
    return 0;
}

static const luaL_Reg gfx_funcs[] = {
    {"color", lua_gfxColor},
    {"lineWidth", lua_gfxLineWidth},
    {"transform", lua_gfxTransform},
    {"save", lua_gfxStateSave},
    {"restore", lua_gfxStateRestore},
    {"clipRect", lua_gfxClipRect},
    {"drawRect", lua_gfxDrawRect},
    {"fillRect", lua_gfxFillRect},
    {"drawLine", lua_gfxDrawLine},
    {"drawImage", lua_gfxDrawImage},
    {"fillText", lua_gfxFillTextAlign},
    {NULL, NULL}
};

// --- Resource Functions ---
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
// through the WASM/C call stack -- including Lua's own interpreter loop
// frames -- back out to the JS caller of Module.ccall(), completely
// bypassing Lua's pcall-based error handling. The C wrappers below convert
// the sentinel into a proper Lua error instead, exactly matching native
// bindings_arcalua.c.
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

static int lua_resourceGetImage(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    float scale = (float)luaL_optnumber(L, 2, 1.0f);
    float centerX = (float)luaL_optnumber(L, 3, 0.0f);
    float centerY = (float)luaL_optnumber(L, 4, 0.0f);
    int filtering = (int)luaL_optinteger(L, 5, 1);
    int handle = js_ResourceGetImage(name, scale, centerX, centerY, filtering);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceCreateImage(lua_State *L) {
    // read uint32_t color data from a table:
    const size_t numItems = lua_istable(L, 1) ? lua_rawlen(L, 1) : 0;
    if (numItems == 0)
        return luaL_error(L, "resource.createImage() expects non-empty table for color data as first argument");
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);
    if (width <= 0 || height <= 0 || (size_t)(width*height) > numItems)
        return luaL_error(L, "resource.createImage() expects positive integers for width and height, and their product must not exceed the number of items");

    uint32_t* data = (uint32_t*)malloc(numItems * sizeof(uint32_t));
    if (!data)
        return luaL_error(L, "resource.createImage() failed to allocate memory for color data");

    for (size_t i = 0; i < numItems; i++) {
        lua_pushinteger(L, i + 1);
        lua_gettable(L, 1);
        data[i] = (uint32_t)luaL_checkinteger(L, -1);
        lua_pop(L, 1);
    }

    float centerX = (float)luaL_optnumber(L, 4, 0.0f);
    float centerY = (float)luaL_optnumber(L, 5, 0.0f);
    int filtering = (int)luaL_optinteger(L, 6, 1);

    int handle = js_ResourceCreateImage((const uint8_t*)data, (int)(numItems * sizeof(uint32_t)), width, height, centerX, centerY, filtering);
    free(data);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceCreateSVGImage(lua_State *L) {
    const char* svg = luaL_checkstring(L, 1);
    float scale = (float)luaL_optnumber(L, 2, 1.0f);
    float centerX = (float)luaL_optnumber(L, 3, 0.0f);
    float centerY = (float)luaL_optnumber(L, 4, 0.0f);
    int handle = js_ResourceCreateSVGImage(svg, scale, centerX, centerY);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceGetTileImage(lua_State *L) {
    uint32_t parent = (uint32_t)luaL_checkinteger(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    int width = (int)luaL_checkinteger(L, 4);
    int height = (int)luaL_checkinteger(L, 5);
    float centerX = (float)luaL_optnumber(L, 6, 0.0f);
    float centerY = (float)luaL_optnumber(L, 7, 0.0f);
    int handle = js_ResourceGetTileImage(parent, x, y, width, height, centerX, centerY);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceGetTileGrid(lua_State *L) {
    uint32_t img = (uint32_t)luaL_checkinteger(L, 1);
    int tilesX = (int)luaL_checkinteger(L, 2);
    int tilesY = (int)luaL_optinteger(L, 3, 1);
    int borderW = (int)luaL_optinteger(L, 4, 0);
    int handle = js_ResourceGetTileGrid(img, tilesX, tilesY, borderW);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceGetAudio(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    int handle = js_ResourceGetAudio(name);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceCreateAudio(lua_State *L) {
    const size_t numSamples = lua_istable(L, 1) ? lua_rawlen(L, 1) : 0;
    if (numSamples == 0)
        return luaL_error(L, "resource.createAudio() expects non-empty table for audio data as first argument");

    float* data = (float*)malloc(numSamples * sizeof(float));
    for (size_t i = 0; i < numSamples; i++) {
        lua_pushinteger(L, i + 1);
        lua_gettable(L, 1);
        data[i] = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    uint8_t numChannels = (uint8_t)luaL_optinteger(L, 2, 1);
    int handle = js_ResourceCreateAudio(data, (int)numSamples, numChannels);
    free(data);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceGetFont(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    uint32_t fontSize = (uint32_t)luaL_optinteger(L, 2, 16);
    int handle = js_ResourceGetFont(name, fontSize);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceQueryImage(lua_State *L) {
    uint32_t image = (uint32_t)luaL_checkinteger(L, 1);
    const char* property = luaL_checkstring(L, 2);
    int value = js_ResourceQueryImage(image, property);
    if(!value)
        return luaL_error(L, "resource.queryImage(%d, '%s') failed: invalid image handle or unrecognized property", image, property);
    lua_pushinteger(L, value);
    return 1;
}

static int lua_resourceQueryAudio(lua_State *L) {
    uint32_t sample = (uint32_t)luaL_checkinteger(L, 1);
    const char* property = luaL_checkstring(L, 2);
    int value = js_ResourceQueryAudio(sample, property);
    if(!value)
        return luaL_error(L, "resource.queryAudio(%d, '%s') failed: invalid audio handle or unrecognized property", sample, property);
    lua_pushinteger(L, value);
    return 1;
}

static int lua_resourceQueryFont(lua_State *L) {
    uint32_t font = (uint32_t)luaL_checkinteger(L, 1);
    const char* property = luaL_checkstring(L, 2);
    const char* str = luaL_optstring(L, 3, "M");
    double value = js_ResourceQueryFont(font, property, str);
    if(isnan(value))
        return luaL_error(L, "resource.queryFont(%d, '%s') failed: invalid font handle or unrecognized property", font, property);
    lua_pushnumber(L, value);
    return 1;
}

static int lua_resourceSetStorageItem(lua_State *L) {
    const char* key = luaL_checkstring(L, 1);
    const char* val = lua_tostring(L, 2);
    js_ResourceSetStorageItem(key, val);
    return 0;
}

static int lua_resourceGetStorageItem(lua_State *L) {
    const char* key = luaL_checkstring(L, 1);
    char* val = js_ResourceGetStorageItem(key);
    if(val) {
        lua_pushstring(L, val);
        free(val);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static const luaL_Reg resource_funcs[] = {
    {"getImage", lua_resourceGetImage},
    {"createImage", lua_resourceCreateImage},
    {"createSVGImage", lua_resourceCreateSVGImage},
    {"getTileImage", lua_resourceGetTileImage},
    {"getTileGrid", lua_resourceGetTileGrid},
    {"getAudio", lua_resourceGetAudio},
    {"createAudio", lua_resourceCreateAudio},
    {"getFont", lua_resourceGetFont},
    {"queryImage", lua_resourceQueryImage},
    {"queryAudio", lua_resourceQueryAudio},
    {"queryFont", lua_resourceQueryFont},
    {"setStorageItem", lua_resourceSetStorageItem},
    {"getStorageItem", lua_resourceGetStorageItem},
    {NULL, NULL}
};

// --- Audio Functions ---
EM_JS(int, js_AudioReplay, (uint32_t sample, float vol, float bal, float det), {
    var track = window.audio.replay(sample, vol, bal, det);
    return (track === undefined) ? -1 : track;
});
EM_JS(void, js_AudioVolume, (uint32_t track, float volume, float fadeTime), {
    window.audio.volume(track, volume, fadeTime);
});

static int lua_AudioReplay(lua_State *L) {
    uint32_t sample = (uint32_t)luaL_checkinteger(L, 1);
    float volume = (float)luaL_optnumber(L, 2, 1.0f);
    float balance = (float)luaL_optnumber(L, 3, 0.0f);
    float detune = (float)luaL_optnumber(L, 4, 0.0f);
    int track = js_AudioReplay(sample, volume, balance, detune);
    lua_pushinteger(L, track);
    return 1;
}

static int lua_AudioVolume(lua_State *L) {
    uint32_t track = (uint32_t)luaL_checkinteger(L, 1);
    float volume = (float)luaL_checknumber(L, 2);
    float fadeTime = (float)luaL_optnumber(L, 3, 0.0f);
    js_AudioVolume(track, volume, fadeTime);
    return 0;
}

static const luaL_Reg audio_funcs[] = {
    {"replay", lua_AudioReplay},
    {"volume", lua_AudioVolume},
    {NULL, NULL}
};

// --- require() support: a custom package.searchers entry that fetches a
// second .lua file the game requires itself (require("helper")), via a
// synchronous XHR -- matching the native runtime's blocking file read.
// This briefly blocks the page for that one fetch (deprecated API, but
// functional everywhere); it's the only way to satisfy Lua's synchronous
// require() semantics without restricting games to single-file scripts.
EM_JS(char*, js_ImportFile, (const char* moduleName), {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', UTF8ToString(moduleName) + '.lua', false);
    xhr.send(null);
    if(xhr.status !== 200)
        return 0;
    return allocateUTF8(xhr.responseText);
});

static int lua_import_searcher(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    char* text = js_ImportFile(name);
    if(!text) {
        lua_pushfstring(L, "\n\tno file '%s.lua' (arcalua web searcher)", name);
        return 1;
    }
    if(luaL_loadbuffer(L, text, strlen(text), name) != LUA_OK) {
        free(text);
        return lua_error(L);
    }
    free(text);
    return 1;
}

// --- Lua Module Registration ---
static void luaopen_arcalua(lua_State *L) {
    luaL_newlib(L, window_funcs);
    lua_setglobal(L, "window");

    luaL_newlib(L, gfx_funcs);
    // do not expose to global but keep them in registry:
    lua_setfield(L, LUA_REGISTRYINDEX, "arcalua_gfx");

    luaL_newlib(L, audio_funcs);
    lua_setglobal(L, "audio");

    luaL_newlib(L, resource_funcs);
    lua_setglobal(L, "resource");

    // register our web searcher after the standard ones (preload/package.path)
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "searchers");
    lua_Integer len = luaL_len(L, -1);
    lua_pushcfunction(L, lua_import_searcher);
    lua_seti(L, -2, len + 1);
    lua_pop(L, 2);
}

// --- Initialization ---

EMSCRIPTEN_KEEPALIVE
void* initVM(const char* script, const char* scriptName) {
    lua_State* L = luaL_newstate();
    if (!L)
        return NULL;
    luaL_openlibs(L);
    luaopen_arcalua(L);

    if (luaL_loadbuffer(L, script, strlen(script), scriptName) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "Error executing script \"%s\": %s\n", scriptName, lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }
    return (void*)L;
}

EMSCRIPTEN_KEEPALIVE
void shutdownVM(void* vm) {
    lua_State* L = (lua_State*)vm;
    if (L)
        lua_close(L);
}

// --- event dispatchers ---
// These only touch the Lua C API (no engine calls), so they're carried over
// from bindings_arcalua.c almost unchanged.

EMSCRIPTEN_KEEPALIVE
bool dispatchLifecycleEvent(const char* evtName, void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, evtName) == LUA_TFUNCTION && lua_pcall(L, 0, 0, 0) != LUA_OK) {
        handleException(L);
        return false;
    }
    return true;
}

EMSCRIPTEN_KEEPALIVE
bool dispatchLifecycleEventArgv(const char* evtName, int argc, char** argv, void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, evtName) != LUA_TFUNCTION)
        return true;

    lua_newtable(L);
    for (int i = 0; i < argc; ++i) {
        lua_pushstring(L, argv[i]);
        lua_seti(L, -2, i + 1);
    }
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        handleException(L);
        return false;
    }
    return true;
}

EMSCRIPTEN_KEEPALIVE
void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "input") != LUA_TFUNCTION)
        return;
    lua_pushstring(L,"axis");
    lua_pushinteger(L, id);
    lua_pushinteger(L, axis);
    lua_pushnumber(L, value);
    lua_pushnil(L);
    if(lua_pcall(L, 5, 0, 0) != LUA_OK)
        handleException(L);
}

EMSCRIPTEN_KEEPALIVE
void dispatchButtonEvent(size_t id, uint8_t button, float value, void* udata) {
    // close-on-buttons-6+7 is handled once, uniformly across languages, by
    // app.js's checkCloseButtons() before this is even called -- no need to
    // duplicate that bookkeeping here.
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "input") != LUA_TFUNCTION)
        return;
    lua_pushstring(L,"button");
    lua_pushinteger(L, id);
    lua_pushinteger(L, button);
    lua_pushnumber(L, value);
    lua_pushnil(L);
    if(lua_pcall(L, 5, 0, 0) != LUA_OK)
        handleException(L);
}

EMSCRIPTEN_KEEPALIVE
bool dispatchUpdateEvent(double deltaT, void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "update") != LUA_TFUNCTION)
        return false;
    lua_pushnumber(L, deltaT);
    if(lua_pcall(L, 1, 1, 0) != LUA_OK) {
        handleException(L);
        return false;
    }
    const bool keepRunning = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return keepRunning;
}

// Deviates from bindings.h's void signature: the browser main loop needs to
// know whether draw() failed (to halt, matching update()'s contract), which
// native doesn't need since a native draw() exception halts via
// WindowEmitClose() + a blocking full-screen error display instead.
EMSCRIPTEN_KEEPALIVE
bool dispatchDrawEvent(void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "draw") != LUA_TFUNCTION)
        return true;
    lua_getfield(L, LUA_REGISTRYINDEX, "arcalua_gfx");
    if(lua_pcall(L, 1, 0, 0) != LUA_OK) {
        handleException(L);
        return false;
    }
    return true;
}
