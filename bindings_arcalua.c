#include "arcamini.h"
#define LUA_IMPL
#include "minilua.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern const char* ResourceArchiveName();

static void handleException(lua_State *L) {
    const char* msg = lua_tostring(L, -1);
    if(!msg)
        msg = "Lua exception (error object is not a string)";
    //luaL_traceback(L, L, msg, 1);
    //msg = lua_tostring(L, -1);
    fprintf(stderr, "--- ERROR ---\n%s\n", msg);
    arcmShowError(msg);
    lua_pop(L, 1);
    WindowEmitClose();
}

// --- Window Functions ---
static int lua_WindowWidth(lua_State *L) {
    lua_pushinteger(L, WindowWidth());
    return 1;
}

static int lua_WindowHeight(lua_State *L) {
    lua_pushinteger(L, WindowHeight());
    return 1;
}

static int lua_WindowClearColor(lua_State *L) {
    uint32_t color = (uint32_t)luaL_checkinteger(L, 1);
    WindowClearColor(color);
    return 0;
}

static const luaL_Reg window_funcs[] = {
    {"width", lua_WindowWidth},
    {"height", lua_WindowHeight},
    {"color", lua_WindowClearColor},
    {NULL, NULL}
};

// --- Graphics Functions ---
static int lua_gfxColor(lua_State *L) {
    uint32_t color = (uint32_t)luaL_checkinteger(L, 1);
    gfxColor(color);
    return 0;
}

static int lua_gfxLineWidth(lua_State *L) {
    float width = (float)luaL_checknumber(L, 1);
    gfxLineWidth(width);
    return 0;
}

static int lua_gfxDrawRect(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    gfxDrawRect(x, y, w, h);
    return 0;
}

static int lua_gfxFillRect(lua_State *L) {
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    float w = (float)luaL_checknumber(L, 3);
    float h = (float)luaL_checknumber(L, 4);
    gfxFillRect(x, y, w, h);
    return 0;
}

static int lua_gfxDrawLine(lua_State *L) {
    float x0 = (float)luaL_checknumber(L, 1);
    float y0 = (float)luaL_checknumber(L, 2);
    float x1 = (float)luaL_checknumber(L, 3);
    float y1 = (float)luaL_checknumber(L, 4);
    gfxDrawLine(x0, y0, x1, y1);
    return 0;
}

static int lua_gfxDrawImage(lua_State *L) {
    uint32_t img = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float rot = (float)luaL_optnumber(L, 4, 0.0f);
    float sc = (float)luaL_optnumber(L, 5, 1.0f);
    int flip = (int)luaL_optnumber(L, 6, 0);
    //printf("gfx.drawImage(%u, %.1f, %.1f, %.1f, %.1f, %i)\n", img, x,y,rot,sc,flip);
    gfxDrawImage(img,(float)x,(float)y,(float)rot,(float)sc,flip);
    return 0;
}

static int lua_gfxFillTextAlign(lua_State *L) {
    uint32_t font = (uint32_t)luaL_checkinteger(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    const char* str = luaL_checkstring(L, 4);
    int align = (int)luaL_optinteger(L, 5, 0);
    gfxFillTextAlign(font, x, y, str, align);
    return 0;
}

static const luaL_Reg gfx_funcs[] = {
    {"color", lua_gfxColor},
    {"lineWidth", lua_gfxLineWidth},
    {"drawRect", lua_gfxDrawRect},
    {"fillRect", lua_gfxFillRect},
    {"drawLine", lua_gfxDrawLine},
    {"drawImage", lua_gfxDrawImage},
    {"fillText", lua_gfxFillTextAlign},
    {NULL, NULL}
};

// --- Resource Functions ---

static int lua_resourceGetImage(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    float scale = (float)luaL_optnumber(L, 2, 1.0f);
    float centerX = (float)luaL_optnumber(L, 3, 0.0f);
    float centerY = (float)luaL_optnumber(L, 4, 0.0f);
    int filtering = (int)luaL_optinteger(L, 5, 1);
    size_t handle = arcmResourceGetImage(name, (float)scale, centerX, centerY, filtering);
    lua_pushinteger(L, handle);
	return 1;
}

static int lua_resourceCreateImage(lua_State *L) {
    // read uint32_t color data from an array:
    const size_t numItems = lua_istable(L, 1) ? lua_rawlen(L, 1) : 0;
    if (numItems == 0)
        return luaL_error(L, "resource.createImage() expects non-empty table for color data as first argument");
    int width = (int)luaL_checkinteger(L, 2);
    int height = (int)luaL_checkinteger(L, 3);
    if (width <= 0 || height <= 0 || width*height > numItems)
        return luaL_error(L, "resource.createImage() expects positive integers for width and height, and their product must not exceed the number of items");

    uint32_t* data = (uint32_t*)malloc(numItems * sizeof(uint32_t));
    if (!data)
        return luaL_error(L, "resource.createImage() failed to allocate memory for color data");

    for (size_t i = 0; i < numItems; i++) {
        lua_pushinteger(L, i + 1);
        lua_gettable(L, 1);
        data[i] = (uint32_t)luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        if(data[i] > 0xFFFFFFFF) {
            free(data);
            return luaL_error(L, "resource.createImage() expects color data to be 32-bit unsigned integers");
        }
    }

    float centerX = (float)luaL_optnumber(L, 4, 0.0f);
    float centerY = (float)luaL_optnumber(L, 5, 0.0f);
    int filtering = (int)luaL_optinteger(L, 6, 1);

    size_t handle = arcmResourceCreateImage((uint8_t*)data, width, height, centerX, centerY, filtering);
    free(data);
    lua_pushinteger(L, handle);
	return 1;
}

static int lua_resourceCreateSVGImage(lua_State *L) {
    const char* svg = luaL_checkstring(L, 1);
    float scale = (float)luaL_optnumber(L, 2, 1.0f);
    float centerX = (float)luaL_optnumber(L, 3, 0.0f);
    float centerY = (float)luaL_optnumber(L, 4, 0.0f);
    size_t handle = arcmResourceCreateSVGImage(svg, scale, centerX, centerY);
    lua_pushinteger(L, handle);
	return 1;
}

static int lua_resourceGetTileGrid(lua_State *L) {
    uint32_t img = (uint32_t)luaL_checkinteger(L, 1);
    int tilesX = (int)luaL_checkinteger(L, 2);
    int tilesY = (int)luaL_optinteger(L, 3, 1);
    int borderW = (int)luaL_optinteger(L, 4, 0);
    size_t handle = gfxImageTileGrid(img, tilesX, tilesY, borderW);
    lua_pushinteger(L, handle);
	return 1;
}

static int lua_resourceGetAudio(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
	size_t handle = ResourceGetAudio(name);
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
        if(data[i] < -1.0f || data[i] > 1.0f) {
            free(data);
            return luaL_error(L, "resource.createAudio() expects audio sample values between -1.0 and 1.0");
        }
    }
    uint8_t numChannels = (uint8_t)luaL_optinteger(L, 2, 1);
    size_t handle = AudioUploadPCM(data, numSamples, numChannels, 0);
    lua_pushinteger(L, handle);
	return 1;
}

static int lua_resourceGetFont(lua_State *L) {
    const char* name = luaL_checkstring(L, 1);
    uint32_t fontSize = (uint32_t)luaL_optinteger(L, 2, 16);
    size_t handle = ResourceGetFont(name, fontSize);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_resourceSetStorageItem(lua_State *L) {
    const char* key = luaL_checkstring(L, 1);
    const char* val = lua_tostring(L, 2);
    arcmResourceSetStorageItem(key, val);
	return 0;
}


static int lua_resourceGetStorageItem(lua_State *L) {
	const char* key = luaL_checkstring(L, 1);
	const char* val = arcmResourceGetStorageItem(key);
    if(val) {
        lua_pushstring(L, val);
    } else {
        lua_pushnil(L);
    }
	return 1;
}

static const luaL_Reg resource_funcs[] = {
    {"getImage", lua_resourceGetImage},
    {"createImage", lua_resourceCreateImage},
    {"createSVGImage", lua_resourceCreateSVGImage},
    {"getTileGrid", lua_resourceGetTileGrid},
    {"getAudio", lua_resourceGetAudio},
    {"createAudio", lua_resourceCreateAudio},
    {"getFont", lua_resourceGetFont},
    {"setStorageItem", lua_resourceSetStorageItem},
    {"getStorageItem", lua_resourceGetStorageItem},
    {NULL, NULL}
};


// --- Audio Functions ---
static int lua_AudioReplay(lua_State *L) {
    uint32_t sample = (uint32_t)luaL_checkinteger(L, 1);
    float volume = (float)luaL_optnumber(L, 2, 1.0f);
    float balance = (float)luaL_optnumber(L, 3, 0.0f);
    float detune = (float)luaL_optnumber(L, 4, 0.0f);
    uint32_t track = AudioReplay(sample, volume, balance, detune);
    lua_pushinteger(L, track);
    return 1;
}

static const luaL_Reg audio_funcs[] = {
    {"replay", lua_AudioReplay},
    {NULL, NULL}
};

// --- Lua Module Registration ---

void luaopen_arcalua(lua_State *L) {
    luaL_newlib(L, window_funcs);
    lua_setglobal(L, "window");

    luaL_newlib(L, gfx_funcs);
    // do not expose to global but keep them in registry:
    lua_setfield(L, LUA_REGISTRYINDEX, "arcalua_gfx");

    luaL_newlib(L, audio_funcs);
    lua_setglobal(L, "audio");

    luaL_newlib(L, resource_funcs);
    lua_setglobal(L, "resource");
}

// --- Initialization ---

void* initVM(const char* script, const char* scriptName) {
    lua_State* L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Failed to create Lua state\n");
        return NULL;
    }
    luaL_openlibs(L);
    luaopen_arcalua(L);

    // add  ResourceArchiveName() to package.path:
    const char* archiveName = ResourceArchiveName();
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    lua_pushfstring(L, ";%s/?.lua;%s/?/init.lua", archiveName, archiveName);
    lua_concat(L, 2);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);

    // execute the passed Lua string
    if (luaL_loadbuffer(L, script, strlen(script), scriptName) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) {
        fprintf(stderr, "Error executing script \"%s\": %s\n", scriptName, lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }
    return (void*)L;
}

void shutdownVM(void* vm) {
    lua_State* L = (lua_State*)vm;
    if (L)
        lua_close(L);
}

// --- event dispatchers ---

bool dispatchLifecycleEvent(const char* evtName, void* udata) {
    lua_State* L = (lua_State*)udata;

    if(lua_getglobal(L, evtName) == LUA_TFUNCTION && lua_pcall(L, 0, 0, 0) != LUA_OK) {
        handleException(L);
        return false;
    }
    return true;
}

void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "input") != LUA_TFUNCTION)
        return;
    // push event:
    lua_pushstring(L,"axis");
    lua_pushinteger(L, id);
    lua_pushinteger(L, axis);
    lua_pushnumber(L, value);
    lua_pushnil(L); // no second value
    if(lua_pcall(L, 5, 0, 0) != LUA_OK)
        handleException(L);
}

void dispatchButtonEvent(size_t id, uint8_t button, float value, void* udata) {
    arcmWindowCloseOnButton67(id, button, value);

    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "input") != LUA_TFUNCTION)
        return;
    // push event:
    lua_pushstring(L,"button");
    lua_pushinteger(L, id);
    lua_pushinteger(L, button);
    lua_pushnumber(L, value);
    lua_pushnil(L); // no second value
    if(lua_pcall(L, 5, 0, 0) != LUA_OK)
        handleException(L);
}

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

void dispatchDrawEvent(void* udata) {
    lua_State* L = (lua_State*)udata;
    if(lua_getglobal(L, "draw") != LUA_TFUNCTION)
        return;
    lua_getfield(L, LUA_REGISTRYINDEX, "arcalua_gfx");
    if(lua_pcall(L, 1, 0, 0) != LUA_OK)
        handleException(L);
}
