#include "bindings.h"
#include "arcamini.h"
#include "quickjs.h"
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>
#include <stdio.h>

extern void* ResourceGetBinary(const char* name, size_t* numBytes);

// --- helper functions ---
static const char* js_print_exception(JSContext *ctx, JSValueConst exc) {
    if (JS_IsNull(exc) || JS_IsUndefined(exc))
        return NULL;
    static char errText[4096];
    size_t pos = 0;

    const char *msg = JS_ToCString(ctx, exc);
    if (msg) {
        fprintf(stderr, "--- ERROR ---\n%s\n", msg);
        pos = snprintf(errText, sizeof(errText), "%s\n\n", msg);
        JS_FreeCString(ctx, msg);
    }

    // Try to get stack trace
    JSValue stack = JS_GetPropertyStr(ctx, exc, "stack");
    if (!JS_IsUndefined(stack)) {
        const char *s = JS_ToCString(ctx, stack);
        if (s) {
            fprintf(stderr, "%s\n", s);
            snprintf(errText + pos, sizeof(errText) - pos, "%s\n", s);
            JS_FreeCString(ctx, s);
        }
    }
    JS_FreeValue(ctx, stack);
    return errText;
}

static void handleException(JSContext *ctx) {
    JSValue exc = JS_GetException(ctx);
    const char *msg = js_print_exception(ctx, exc);
    if(msg)
        arcmShowError(msg);
    JS_FreeValue(ctx, exc);
    WindowEmitClose();
}

static int JS_ToFloat64Default(JSContext *ctx, double *pres, JSValueConst val, double defaultValue) {
    if (JS_IsUndefined(val)) {
        *pres = defaultValue;
        return 0;
    }
    return JS_ToFloat64(ctx, pres, val);
}
static int JS_ToInt32Default(JSContext *ctx, int32_t *pres, JSValueConst val, int32_t defaultValue) {
    if (JS_IsUndefined(val)) {
        *pres = defaultValue;
        return 0;
    }
    return JS_ToInt32(ctx, pres, val);
}

static int JS_ToUint32Default(JSContext *ctx, uint32_t *pres, JSValueConst val, uint32_t defaultValue) {
    if (JS_IsUndefined(val)) {
        *pres = defaultValue;
        return 0;
    }
    return JS_ToUint32(ctx, pres, val);
}

// Returns pointer to raw bytes if `val` is an ArrayBuffer or TypedArray.
// Fills out *plen and *pelem_size if provided.
// Returns NULL if not valid.
//
// NOTE: Returned pointer is only valid as long as `val` (or its underlying
// ArrayBuffer) is alive in JS. You must NOT free it yourself.
static uint8_t *qjs_get_bytes(JSContext *ctx, JSValueConst val, size_t *plen, size_t *pelem_size) {
    // Try ArrayBuffer directly
    {
        size_t size;
        uint8_t *ptr = JS_GetArrayBuffer(ctx, &size, val);
        if (ptr) {
            if (plen) *plen = size;
            if (pelem_size) *pelem_size = 0;  // pure buffer, no element size
            return ptr;
        }
    }

    // Try TypedArray view
    {
        size_t offset, length, elem_size;
        JSValue buf = JS_GetTypedArrayBuffer(ctx, val,
                                             &offset, &length, &elem_size);
        if (!JS_IsException(buf)) {
            size_t buf_size;
            uint8_t *base = JS_GetArrayBuffer(ctx, &buf_size, buf);
            uint8_t *ptr = NULL;
            if (base) {
                ptr = base + offset;
                if (plen) *plen = length;
                if (pelem_size) *pelem_size = elem_size;
            }
            JS_FreeValue(ctx, buf);
            return ptr;
        }
    }
    // Not an ArrayBuffer or TypedArray
    return NULL;
}

static size_t getArrayLength(JSContext *ctx, JSValueConst arr) {
    JSValue len_val = JS_GetPropertyStr(ctx, arr, "length");
    int64_t len;
    if (JS_ToInt64(ctx, &len, len_val))
        len = 0;
    JS_FreeValue(ctx, len_val);
    return len;
}

static uint32_t* getUint32Array(JSContext *ctx, JSValueConst arr, size_t *num_elems) {
    if(num_elems)
        *num_elems = 0;
    if (!JS_IsArray(ctx, arr))
        return NULL;

    const size_t len = getArrayLength(ctx, arr);
    if (!len)
        return NULL;

    uint32_t *buf = (uint32_t*)malloc(sizeof(uint32_t) * len);
    if (!buf)
        return NULL; // allocation failed

    for (size_t i = 0; i < len; i++) {
        JSValue elem = JS_GetPropertyUint32(ctx, arr, i);
        if (!JS_IsNumber(elem) || JS_ToUint32(ctx, &buf[i], elem) != 0) { // try to convert to uint32
            fprintf(stderr, "failed to read Uint32 array element %zu\n", i);
            JS_FreeValue(ctx, elem);
            free(buf);
            return NULL;
        }
        JS_FreeValue(ctx, elem);
    }

    if(num_elems)
        *num_elems = len;
    return buf;
}

static float* getFloatArray(JSContext *ctx, JSValueConst arr, size_t *num_elems) {
    if(num_elems)
        *num_elems = 0;
    if (!JS_IsArray(ctx, arr))
        return NULL;

    const size_t len = getArrayLength(ctx, arr);
    if (!len)
        return NULL;

    float *buf = (float*)malloc(sizeof(float) * len);
    if (!buf)
        return NULL; // allocation failed

    double value;
    for (size_t i = 0; i < len; i++) {
        JSValue elem = JS_GetPropertyUint32(ctx, arr, i);
        if (!JS_IsNumber(elem) || JS_ToFloat64(ctx, &value, elem) != 0) { // try to convert to double
            fprintf(stderr, "failed to read array element %zu\n", i);
            JS_FreeValue(ctx, elem);
            free(buf);
            return NULL;
        }
        buf[i] = value;
        JS_FreeValue(ctx, elem);
    }

    if(num_elems)
        *num_elems = len;
    return buf;
}

static JSValue require_cache_lookup(JSContext *ctx, const char* key) {
    //fprintf(stderr, "Module cache lookup: %s\n", key);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue require_func = JS_GetPropertyStr(ctx, global, "require");
    JSValue cache = JS_GetPropertyStr(ctx, require_func, "_cache");
    JS_FreeValue(ctx, require_func);
    JS_FreeValue(ctx, global);
    if (!JS_IsUndefined(cache)) {
        JSValue cached = JS_GetPropertyStr(ctx, cache, key);
        JS_FreeValue(ctx, cache);
        //fprintf(stderr, "Module cache lookup: %s %s\n", key , JS_IsUndefined(cached) ? "miss" : "hit");
        return cached; // owned by caller
    }
    return JS_UNDEFINED;
}

static void require_cache_insert(JSContext *ctx, const char* key, JSValue val) {
    //fprintf(stderr, "Module cache insert: %s\n", key);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue require_func = JS_GetPropertyStr(ctx, global, "require");
    JSValue cache = JS_GetPropertyStr(ctx, require_func, "_cache");
    if (JS_IsUndefined(cache)) {
        cache = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, require_func, "_cache", JS_DupValue(ctx, cache));
    }
    JS_SetPropertyStr(ctx, cache, key, JS_DupValue(ctx, val));
    JS_FreeValue(ctx, cache);
    JS_FreeValue(ctx, require_func);
    JS_FreeValue(ctx, global);
}

// Load a script file and evaluate it
static JSValue require(JSContext *ctx, const char *filename) {
    if(filename && filename[0] == '.' && filename[1] == '/') {
        filename += 2;
    }
    // Check cache first
    JSValue cached = require_cache_lookup(ctx, filename);
    if (!JS_IsUndefined(cached))
        return cached;

    size_t script_len;
    char* script = (char*)ResourceGetBinary(filename, &script_len); 
    if (!script) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return JS_UNDEFINED;
    }

    const char wrapper_prefix[] = "(function(module) { let exports = module.exports; ";
    const char wrapper_suffix[] = "; return module.exports; })({exports: {}});";
    const size_t wrapper_prefix_len = sizeof(wrapper_prefix) - 1;
    const size_t wrapper_suffix_len = sizeof(wrapper_suffix) - 1;
    const size_t buf_len = wrapper_prefix_len + script_len + wrapper_suffix_len;

    char *buf = malloc(buf_len+1);
    if (!buf) {
        free(script);
        fprintf(stderr, "Memory allocation failed\n");
        return JS_UNDEFINED;
    }

    memcpy(buf, wrapper_prefix, wrapper_prefix_len);
    memcpy(buf + wrapper_prefix_len, script, script_len);
    free(script);
    memcpy(buf + wrapper_prefix_len + script_len, wrapper_suffix, wrapper_suffix_len);
    buf[buf_len] = '\0';

    // Insert placeholder before evaluation in case of circular dependencies
    JSValue placeholder = JS_NewObject(ctx);
    require_cache_insert(ctx, filename, placeholder);
    JS_FreeValue(ctx, placeholder); // cache keeps its dup

    JSValue ret = JS_Eval(ctx, buf, buf_len, filename, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
    free(buf);

    if (JS_IsException(ret)) {
        handleException(ctx);
        JS_FreeValue(ctx, ret);
        return JS_UNDEFINED;
    }
    require_cache_insert(ctx, filename, ret);
    return ret;
}

static JSValue js_require(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_UNDEFINED;

    const char *filename = JS_ToCString(ctx, argv[0]);
    if (!filename) return JS_UNDEFINED;

    JSValue exports_obj = require(ctx, filename);  // call our C-side require()
    JS_FreeCString(ctx, filename);
    return exports_obj;
}

// --- Window bindings ---

static JSValue js_WindowTitle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *str = JS_ToCString(ctx, argv[0]);
    if (!str)
        return JS_ThrowTypeError(ctx, "WindowTitle expects a string");
    WindowTitle(str);
    JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

static JSValue js_WindowWidth(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewInt32(ctx, WindowWidth());
}

static JSValue js_WindowHeight(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return JS_NewInt32(ctx, WindowHeight());
}

static JSValue js_WindowClearColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t color = 0;
    if (JS_ToUint32(ctx, &color, argv[0]) || color == 0)
        return JS_ThrowTypeError(ctx, "window.color(value) expects a non-zero integer color value");
    WindowClearColor(color);
    return JS_UNDEFINED;
}

// --- window.switchScene binding ---
static JSValue js_WindowSwitchScene(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *fname = argc ? JS_ToCString(ctx, argv[0]) : NULL;
    if (!fname)
        return JS_ThrowTypeError(ctx, "window.switchScene: invalid or missing filename");

    // Collect additional arguments
    int numArgs = argc - 1;
    char **args = NULL;
    if (numArgs > 0) {
        args = (char **)malloc(numArgs * sizeof(char *));
        for (int i = 0; i < numArgs; ++i) {
            const char *argStr = JS_ToCString(ctx, argv[i+1]);
            if (!argStr) {
                JS_FreeCString(ctx, fname);
                for (int j = 0; j < i; ++j) free(args[j]);
                free(args);
                return JS_ThrowTypeError(ctx, "window.switchScene: invalid argument");
            }
            args[i] = strdup(argStr);
            JS_FreeCString(ctx, argStr);
        }
    }

    // Call leave event on current script
    dispatchLifecycleEvent("leave", ctx);
    // Clear current callbacks:
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "enter", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, global, "input", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, global, "update", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, global, "draw", JS_UNDEFINED);
    JS_SetPropertyStr(ctx, global, "leave", JS_UNDEFINED);
    JS_FreeValue(ctx, global);

    // Load new script
    char *script = (char *)ResourceGetText(fname);
    if (!script) {
        JS_FreeCString(ctx, fname);
        if (args) {
            for (int i = 0; i < numArgs; ++i) free(args[i]);
            free(args);
        }
        return JS_ThrowReferenceError(ctx, "window.switchScene: file not found");
    }

    // Evaluate new script
    JSValue ret = JS_Eval(ctx, script, strlen(script), fname, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);
    free(script);

    JS_FreeCString(ctx, fname);
    if (JS_IsException(ret)) {
        handleException(ctx);
        JS_FreeValue(ctx, ret);
    }
    else dispatchLifecycleEventArgv("enter", numArgs, args, ctx); // Dispatch enter event with arguments
    if (args) {
        for (int i = 0; i < numArgs; ++i) free(args[i]);
        free(args);
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_Window_funcs[] = {
    JS_CFUNC_DEF("title", 1, js_WindowTitle),
    JS_CFUNC_DEF("width", 0, js_WindowWidth),
    JS_CFUNC_DEF("height", 0, js_WindowHeight),
    JS_CFUNC_DEF("color", 1, js_WindowClearColor),
    JS_CFUNC_DEF("switchScene", 1, js_WindowSwitchScene),
};


// --- gfx bindings ---
static JSValue gfx_ns;

static JSValue js_gfxColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t c;
    if (JS_ToUint32(ctx, &c, argv[0]))
        return JS_ThrowTypeError(ctx, "gfx.color expects integer color");
    gfxColor(c);
    return JS_UNDEFINED;
}

static JSValue js_gfxLineWidth(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double w;
    if (JS_ToFloat64(ctx, &w, argv[0]))
        return JS_ThrowTypeError(ctx, "gfx.lineWidth expects number");
    gfxLineWidth((float)w);
    return JS_UNDEFINED;
}

static JSValue js_gfxTransform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double x, y, rot, sc;
    if (JS_ToFloat64(ctx, &x, argv[0]) ||
        JS_ToFloat64(ctx, &y, argv[1]) ||
        JS_ToFloat64Default(ctx, &rot, argv[2], 0.0) ||
        JS_ToFloat64Default(ctx, &sc, argv[3], 1.0))
        return JS_ThrowTypeError(ctx, "gfx.transform expects 4 numbers");
    gfxTransform((float)x,(float)y,(float)rot,(float)sc);
    return JS_UNDEFINED;
}

static JSValue js_gfxStateSave(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    gfxStateSave();
    return JS_UNDEFINED;
}

static JSValue js_gfxStateRestore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    gfxStateRestore();
    return JS_UNDEFINED;
}

static JSValue js_gfxClipRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int32_t x,y,w,h;
    if (JS_ToInt32(ctx, &x, argv[0]) ||
        JS_ToInt32(ctx, &y, argv[1]) ||
        JS_ToInt32(ctx, &w, argv[2]) ||
        JS_ToInt32(ctx, &h, argv[3]))
        return JS_ThrowTypeError(ctx, "gfx.clipRect expects 4 integers");
    gfxClipRect(x,y,w,h);
    return JS_UNDEFINED;
}

static JSValue js_gfxDrawRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double x,y,w,h;
    if (JS_ToFloat64(ctx, &x, argv[0]) ||
        JS_ToFloat64(ctx, &y, argv[1]) ||
        JS_ToFloat64(ctx, &w, argv[2]) ||
        JS_ToFloat64(ctx, &h, argv[3]))
        return JS_ThrowTypeError(ctx, "gfx.drawRect expects 4 numbers");
    gfxDrawRect((float)x,(float)y,(float)w,(float)h);
    return JS_UNDEFINED;
}

static JSValue js_gfxFillRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double x,y,w,h;
    if (JS_ToFloat64(ctx, &x, argv[0]) ||
        JS_ToFloat64(ctx, &y, argv[1]) ||
        JS_ToFloat64(ctx, &w, argv[2]) ||
        JS_ToFloat64(ctx, &h, argv[3]))
        return JS_ThrowTypeError(ctx, "gfx.fillRect expects 4 numbers");
    gfxFillRect((float)x,(float)y,(float)w,(float)h);
    return JS_UNDEFINED;
}

static JSValue js_gfxDrawLine(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    double x0,y0,x1,y1;
    if (JS_ToFloat64(ctx, &x0, argv[0]) ||
        JS_ToFloat64(ctx, &y0, argv[1]) ||
        JS_ToFloat64(ctx, &x1, argv[2]) ||
        JS_ToFloat64(ctx, &y1, argv[3]))
        return JS_ThrowTypeError(ctx, "gfx.drawLine expects 4 numbers");
    gfxDrawLine((float)x0,(float)y0,(float)x1,(float)y1);
    return JS_UNDEFINED;
}

static JSValue js_gfxDrawImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t img; double x,y,rot,sc; int flip;
    if (JS_ToUint32(ctx, &img, argv[0]) ||
        JS_ToFloat64(ctx, &x, argv[1]) ||
        JS_ToFloat64(ctx, &y, argv[2]) ||
        JS_ToFloat64Default(ctx, &rot, argv[3], 0.0) ||
        JS_ToFloat64Default(ctx, &sc, argv[4], 1.0) ||
        JS_ToInt32Default(ctx, &flip, argv[5], 0))
        return JS_ThrowTypeError(ctx, "gfx.drawImage expects (uint32, number, number, [number, number, int])");
    //printf("gfx.drawImage(%u, %.1f, %.1f, %.1f, %.1f, %i)\n", img, x,y,rot,sc,flip);
    gfxDrawImage(img,(float)x,(float)y,(float)rot,(float)sc,flip);
    return JS_UNDEFINED;
}

static JSValue js_gfxFillTextAlign(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t font; double x,y; int align;
    const char* str = JS_ToCString(ctx, argv[3]);
    if (!str ||
        JS_ToUint32(ctx, &font, argv[0]) ||
        JS_ToFloat64(ctx, &x, argv[1]) ||
        JS_ToFloat64(ctx, &y, argv[2]) ||
        JS_ToInt32Default(ctx, &align, argv[4], 0)) {
        if (str) JS_FreeCString(ctx, str);
        return JS_ThrowTypeError(ctx, "gfx.fillText expects (uint32, number, number, string, [int])");
    }
    gfxFillTextAlign(font,(float)x,(float)y,str,align);
    JS_FreeCString(ctx, str);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_gfx_funcs[] = {
    JS_CFUNC_DEF("color", 1, js_gfxColor),
    JS_CFUNC_DEF("lineWidth", 1, js_gfxLineWidth),
    JS_CFUNC_DEF("transform", 4, js_gfxTransform),
    JS_CFUNC_DEF("save", 0, js_gfxStateSave),
    JS_CFUNC_DEF("restore", 0, js_gfxStateRestore),
    JS_CFUNC_DEF("clipRect", 4, js_gfxClipRect),
    JS_CFUNC_DEF("drawRect", 4, js_gfxDrawRect),
    JS_CFUNC_DEF("fillRect", 4, js_gfxFillRect),
    JS_CFUNC_DEF("drawLine", 4, js_gfxDrawLine),
    JS_CFUNC_DEF("drawImage", 6, js_gfxDrawImage),
    JS_CFUNC_DEF("fillText", 5, js_gfxFillTextAlign),
};


// --- Audio bindings ---

static JSValue js_AudioReplay(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    uint32_t sample; double vol, bal, det;
    if (JS_ToUint32(ctx, &sample, argv[0]) ||
        JS_ToFloat64Default(ctx, &vol, argv[1], 1.0) ||
        JS_ToFloat64Default(ctx, &bal, argv[2], 0.0) ||
        JS_ToFloat64Default(ctx, &det, argv[3], 0.0))
        return JS_ThrowTypeError(ctx, "audio.replay expects (uint32[, number, number, number])");
    uint32_t track = AudioReplay(sample,(float)vol,(float)bal,(float)det);
    if (track == UINT_MAX)
        return JS_UNDEFINED; // signify invalid
    return JS_NewUint32(ctx, track);
}

static JSValue js_AudioVolume(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    uint32_t track; double vol, fadeTime;
    if (JS_ToUint32(ctx, &track, argv[0]) ||
        JS_ToFloat64(ctx, &vol, argv[1]) ||
        JS_ToFloat64Default(ctx, &fadeTime, argv[2], 0.0))
        return JS_ThrowTypeError(ctx, "audio.volume expects (uint32, number[, number])");
    arcmAudioVolume(track,(float)vol,(float)fadeTime);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_Audio_funcs[] = {
    JS_CFUNC_DEF("replay", 4, js_AudioReplay),
    JS_CFUNC_DEF("volume", 3, js_AudioVolume),
};


// --- Resource bindings ---

static JSValue js_ResourceGetImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char* name = JS_ToCString(ctx, argv[0]);
    double scale, centerX, centerY; int filtering;
    if (!name || JS_ToFloat64Default(ctx, &scale, argv[1], 1.0)
        || JS_ToFloat64Default(ctx, &centerX, argv[2], 0.0)
        || JS_ToFloat64Default(ctx, &centerY, argv[3], 0.0)
        || JS_ToInt32Default(ctx, &filtering, argv[4], 1)) {
        if (name) JS_FreeCString(ctx, name);
        return JS_ThrowTypeError(ctx, "resource.getImage expects (string[, number, number, number, int])");
    }
    size_t handle = arcmResourceGetImage(name, (float)scale, centerX, centerY, filtering);
    JS_FreeCString(ctx, name);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

static JSValue js_ResourceCreateSVGImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char* svg = JS_ToCString(ctx, argv[0]);
    double scale, centerX, centerY;
    if(!svg || JS_ToFloat64Default(ctx, &scale, argv[1], 1.0)
        || JS_ToFloat64Default(ctx, &centerX, argv[2], 0.0)
        || JS_ToFloat64Default(ctx, &centerY, argv[3], 0.0)) {
        JS_FreeCString(ctx, svg);
        return JS_ThrowTypeError(ctx, "resource.createSVGImage expects (string[, number, number, number])");
    }
    size_t handle = arcmResourceCreateSVGImage(svg, scale, centerX, centerY);
    JS_FreeCString(ctx, svg);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

static JSValue js_ResourceCreateImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint8_t* alloc_data = NULL;
    size_t bufSz=0;
    if (JS_IsArray(ctx, argv[0])) { // read from array
        alloc_data = (uint8_t*)getUint32Array(ctx, argv[0], &bufSz);
        if(alloc_data)
            bufSz *= sizeof(uint32_t);
    }
    // read data from an array buffer:
    const uint8_t* data = alloc_data ? alloc_data : qjs_get_bytes(ctx, argv[0], &bufSz, NULL);
    if (!data || !bufSz) {
        free(alloc_data);
        return JS_ThrowTypeError(ctx, "resource.createImage expects non-empty array, Uint32Array or ArrayBuffer as first argument");
    }
    int width, height, filtering; double centerX, centerY;
    if (JS_ToInt32(ctx, &width, argv[1])|| JS_ToInt32(ctx, &height, argv[2])
        || JS_ToFloat64Default(ctx, &centerX, argv[3], 0.0) || JS_ToFloat64Default(ctx, &centerY, argv[4], 0.0)
        || JS_ToInt32Default(ctx, &filtering, argv[5], 1)) {
        free(alloc_data);
        return JS_ThrowTypeError(ctx, "resource.createImage expects (ArrayBuffer, int, int[, float, float, int])");
    }
    size_t handle = arcmResourceCreateImage(data, width, height, centerX, centerY, filtering);
    free(alloc_data);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

static JSValue js_ResourceGetTileGrid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t img; uint32_t tilesX, tilesY, borderW;
    if (JS_ToUint32(ctx, &img, argv[0])
        || JS_ToUint32(ctx, &tilesX, argv[1])
        || JS_ToUint32Default(ctx, &tilesY, argv[2], 1)
        || JS_ToUint32Default(ctx, &borderW, argv[3], 0)) {
        return JS_ThrowTypeError(ctx, "resource.getTileGrid expects (uint32, uint32[, uint32, uint32])");
    }
    size_t handle = gfxImageTileGrid(img, tilesX, tilesY, borderW);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

static JSValue js_ResourceGetAudio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char* name = JS_ToCString(ctx, argv[0]);
    if (!name)
        return JS_ThrowTypeError(ctx, "resource.getAudio expects string");
    size_t handle = ResourceGetAudio(name);
    JS_FreeCString(ctx, name);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

// binding for uint32_t AudioUploadPCM(float* waveData, uint32_t numSamples, uint8_t numChannels, uint32_t offset)
static JSValue js_ResourceCreateAudio(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    float* alloc_data = NULL;
    size_t bufSz=0;
    if (JS_IsArray(ctx, argv[0])) { // read from array
        alloc_data = getFloatArray(ctx, argv[0], &bufSz);
        if(alloc_data)
            bufSz *= sizeof(float);
    }
    // read data from an array buffer:
    const float* data = alloc_data ? alloc_data : (float*)qjs_get_bytes(ctx, argv[0], &bufSz, NULL);
    if (!data || !bufSz || (bufSz % sizeof(float)) != 0) {
        free(alloc_data);
        return JS_ThrowTypeError(ctx, "resource.createAudio expects non-empty array, Float32Array or ArrayBuffer as first argument");
    }

    uint32_t numSamples, numChannels;
    if (JS_ToUint32Default(ctx, &numChannels, argv[1], 1)) {
        free(alloc_data);
        return JS_ThrowTypeError(ctx, "resource.createAudio expects (array[, uint32])");
    }
    if (numChannels == 0 || numChannels > 2) {
        free(alloc_data);
        return JS_ThrowTypeError(ctx, "resource.createAudio: numChannels must be 1 or 2");
    }
    numSamples = bufSz / sizeof(float) / numChannels;

    if(!alloc_data) { // copy data before uploading, memory of copy is then owned by AudioUploadPCM
        alloc_data = (float*)malloc(bufSz);
        if (!alloc_data) return JS_ThrowOutOfMemory(ctx);
        memcpy(alloc_data, data, bufSz);
    }
    size_t handle = AudioUploadPCM(alloc_data, numSamples, numChannels, 0);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

static JSValue js_ResourceGetFont(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char* name = JS_ToCString(ctx, argv[0]);
    uint32_t fontSize;
    if (!name || JS_ToUint32(ctx, &fontSize, argv[1])) {
        if (name) JS_FreeCString(ctx, name);
        return JS_ThrowTypeError(ctx, "resource.getFont expects (string, uint32)");
    }
    size_t handle = ResourceGetFont(name, fontSize);
    JS_FreeCString(ctx, name);
    return JS_NewUint32(ctx, (uint32_t)handle);
}

static JSValue js_ResourceGetStorageItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char* key = JS_ToCString(ctx, argv[0]);
    if (!key)
        return JS_ThrowTypeError(ctx, "resource.getStorageItem expects string key");
    const char* val = arcmResourceGetStorageItem(key);
    JS_FreeCString(ctx, key);
    if (!val) return JS_NULL;
    return JS_NewString(ctx, val);
}

static JSValue js_ResourceSetStorageItem(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char* key = JS_ToCString(ctx, argv[0]);
    const char* val = JS_ToCString(ctx, argv[1]);
    if (!key || !val) {
        if (key) JS_FreeCString(ctx, key);
        if (val) JS_FreeCString(ctx, val);
        return JS_ThrowTypeError(ctx, "resource.setStorageItem expects (string, string)");
    }
    arcmResourceSetStorageItem(key, val);
    JS_FreeCString(ctx, key);
    JS_FreeCString(ctx, val);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_Resource_funcs[] = {
    JS_CFUNC_DEF("getImage", 5, js_ResourceGetImage),
    JS_CFUNC_DEF("createSVGImage", 4, js_ResourceCreateSVGImage),
    JS_CFUNC_DEF("createImage", 6, js_ResourceCreateImage),
    JS_CFUNC_DEF("getTileGrid", 4, js_ResourceGetTileGrid),
    JS_CFUNC_DEF("getAudio", 1, js_ResourceGetAudio),
    JS_CFUNC_DEF("createAudio", 4, js_ResourceCreateAudio),
    JS_CFUNC_DEF("getFont", 2, js_ResourceGetFont),
    JS_CFUNC_DEF("getStorageItem", 1, js_ResourceGetStorageItem),
    JS_CFUNC_DEF("setStorageItem", 2, js_ResourceSetStorageItem),
};

static void bindArcamini(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    JSValue window_ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, window_ns, js_Window_funcs,
                               sizeof(js_Window_funcs)/sizeof(JSCFunctionListEntry));
    JS_SetPropertyStr(ctx, global, "window", window_ns);

    gfx_ns = JS_NewObject(ctx);
    JS_DupValue(ctx, gfx_ns); // increment ref count
    JS_SetPropertyFunctionList(ctx, gfx_ns, js_gfx_funcs,
                               sizeof(js_gfx_funcs)/sizeof(JSCFunctionListEntry));
    JS_SetPropertyStr(ctx, global, "gfx", gfx_ns);

    JSValue audio_ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, audio_ns, js_Audio_funcs,
                               sizeof(js_Audio_funcs)/sizeof(JSCFunctionListEntry));
    JS_SetPropertyStr(ctx, global, "audio", audio_ns);

    JSValue resource_ns = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, resource_ns, js_Resource_funcs,
                               sizeof(js_Resource_funcs)/sizeof(JSCFunctionListEntry));
    JS_SetPropertyStr(ctx, global, "resource", resource_ns);

    JS_FreeValue(ctx, global);
}


// --- Minimal console binding ---
static JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        // print objects and arrays as JSON
        if (JS_IsObject(argv[i])) {
            const char *json = JS_ToCString(ctx, JS_JSONStringify(ctx, argv[i], JS_UNDEFINED, JS_UNDEFINED));
            if (json) {
                fprintf(stdout, "%s%s", json, (i == argc - 1) ? "" : " ");
                JS_FreeCString(ctx, json);
            }
        } else {
            const char *str = JS_ToCString(ctx, argv[i]);
            if (str) {
                fprintf(stdout, "%s%s", str, (i == argc - 1) ? "" : " ");
                JS_FreeCString(ctx, str);
            }
            else { // print typeof in parentheses
                const char *type = JS_ToCString(ctx, JS_GetPropertyStr(ctx, argv[i], "constructor"));
                if (type) {
                    fprintf(stdout, "(%s)", type);
                    JS_FreeCString(ctx, type);
                }
            }
        }
    }
    fprintf(stdout, "\n");
    return JS_UNDEFINED;
}
static JSValue js_console_err(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    for (int i = 0; i < argc; i++) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            fprintf(stderr, "%s%s", str, (i == argc - 1) ? "" : " ");
            JS_FreeCString(ctx, str);
        }
    }
    fprintf(stderr, "\n");
    return JS_UNDEFINED;
}


static void bindConsoleRequire(JSContext *ctx) {
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, js_console_err, "error", 1));
    JS_SetPropertyStr(ctx, console, "warn", JS_NewCFunction(ctx, js_console_err, "warn", 1));
    JS_SetPropertyStr(ctx, global, "console", console);
    JS_SetPropertyStr(ctx, global, "require", JS_NewCFunction(ctx, js_require, "require", 1));

    JS_FreeValue(ctx, global);
}

// --- Initialization ---
void* initVM(const char* script, const char* scriptName) {
    JSRuntime* rt = JS_NewRuntime();
    if (!rt) return NULL;

    JSContext* ctx = JS_NewContext(rt);
    if (!ctx) {
        JS_FreeRuntime(rt);
        return NULL;
    }

    // Initialize our bindings
    bindArcamini(ctx);
    bindConsoleRequire(ctx);

    // Evaluate user script
    JSValue main_ns = JS_Eval(ctx, script, strlen(script), scriptName,
        JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT);

    if (JS_IsException(main_ns)) {
        JSValue exc = JS_GetException(ctx);
        js_print_exception(ctx, exc);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, main_ns);
        shutdownVM(ctx);
        return NULL;
    }

    JS_FreeValue(ctx, main_ns);
    return (void*)ctx;
}

void shutdownVM(void* context) {
    if (!context) return;
    JSContext* ctx = (JSContext*)context;
    JSRuntime* rt = JS_GetRuntime(ctx);
    JS_FreeValue(ctx, gfx_ns);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
}

// --- event dispatchers ---

bool dispatchLifecycleEvent(const char* evtName, void* callback) {
    JSContext *ctx = (JSContext*)callback;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, evtName);
    bool ok = true;
    if (JS_IsFunction(ctx, fn)) {
        JSValue ret = JS_Call(ctx, fn, global, 0, NULL);
        if (JS_IsException(ret)) {
            handleException(ctx);
            ok = false;
        }
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return ok;
}

bool dispatchLifecycleEventArgv(const char* evtName, int argc, char** argv, void* callback) {
    JSContext *ctx = (JSContext*)callback;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, evtName);
    bool ok = true;
    if (JS_IsFunction(ctx, fn)) {
        JSValue args_array = JS_NewArray(ctx);
        for (int i = 0; i < argc; ++i)
            JS_SetPropertyUint32(ctx, args_array, i, JS_NewString(ctx, argv[i]));
        JSValue argv[1] = { args_array };
        JSValue ret = JS_Call(ctx, fn, global, 1, argv);
        if (JS_IsException(ret)) {
            handleException(ctx);
            ok = false;
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, args_array);
    }
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return ok;
}

void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* callback) {
    JSContext *ctx = (JSContext*)callback;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "input");
    if (JS_IsFunction(ctx, fn)) {
        JSValue evt = JS_NewString(ctx, "axis");
        JSValue device = JS_NewUint32(ctx, (uint32_t)id);
        JSValue id = JS_NewUint32(ctx, axis);
        JSValue val = JS_NewFloat64(ctx, value);
        JSValue val2 = JS_UNDEFINED;

        JSValue argv[5] = { evt, device, id, val, val2 };
        JSValue ret = JS_Call(ctx, fn, global, 5, argv);
        if (JS_IsException(ret))
            handleException(ctx);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, evt);
        JS_FreeValue(ctx, device);
        JS_FreeValue(ctx, id);
        JS_FreeValue(ctx, val);
    }
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
}

void dispatchButtonEvent(size_t id, uint8_t button, float value, void* callback) {
    JSContext *ctx = (JSContext*)callback;
    arcmWindowCloseOnButton67(id, button, value);

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "input");
    if (JS_IsFunction(ctx, fn)) {
        JSValue evt = JS_NewString(ctx, "button");
        JSValue device = JS_NewUint32(ctx, (uint32_t)id);
        JSValue id = JS_NewUint32(ctx, button);
        JSValue val = JS_NewFloat64(ctx, value);
        JSValue val2 = JS_UNDEFINED;

        JSValue argv[5] = { evt, device, id, val, val2 };
        JSValue ret = JS_Call(ctx, fn, global, 5, argv);
        if (JS_IsException(ret))
            handleException(ctx);
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, evt);
        JS_FreeValue(ctx, device);
        JS_FreeValue(ctx, id);
        JS_FreeValue(ctx, val);
    }
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
}

bool dispatchUpdateEvent(double deltaT, void* callback) {
    JSContext *ctx = (JSContext*)callback;
    JSValue global = JS_GetGlobalObject(ctx);
    //JS_ExecutePendingJob(JS_GetRuntime(ctx), &ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "update");
    bool keepRunning = false;
    if (JS_IsFunction(ctx, fn)) {
        JSValue argv[1] = { JS_NewFloat64(ctx, deltaT) };
        JSValue ret = JS_Call(ctx, fn, global, 1, argv);
        if (JS_IsException(ret))
            handleException(ctx);
        else {
            int b = JS_ToBool(ctx, ret);
            if (b >= 0) keepRunning = b;
        }
        JS_FreeValue(ctx, argv[0]);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
    return keepRunning;
}

void dispatchDrawEvent(void* callback) {
    JSContext *ctx = (JSContext*)callback;
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_GetPropertyStr(ctx, global, "draw");
    if (JS_IsFunction(ctx, fn)) {
        JSValue argv[1] = { gfx_ns }; // pass gfx namespace as argument
        JSValue ret = JS_Call(ctx, fn, global, 1, argv);
        if (JS_IsException(ret))
            handleException(ctx);
        JS_FreeValue(ctx, ret);
    }
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, global);
}
