#include "bindings.h"
#include "pocketpy.h"
#include "arcamini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool handleException() {
	char* msg = py_formatexc();
	if(msg) {
		fprintf(stderr, "--- ERROR ---\n%s\n", msg);
		// todo: forward exception to debugger if attached
		arcmShowError(msg);
		free(msg);
	}
	return false;
}

// --- window bindings ---
static bool py_WindowWidth(int argc, py_StackRef argv) {
	py_newint(py_retval(), WindowWidth());
	return true;
}
static bool py_WindowHeight(int argc, py_StackRef argv) {
	py_newint(py_retval(), WindowHeight());
	return true;
}
static bool py_WindowClearColor(int argc, py_StackRef argv) {
	int64_t color;
	if(!py_castint(py_arg(0), &color))
		return false;

	WindowClearColor((uint32_t)color);
	py_newnone(py_retval());
	return true;
}

// experimental switchScene() implementation for supporting multiple scenes
static bool py_switchScene(int argc, py_StackRef argv) {
	const char* fname = py_tostr(py_arg(0));
	const char** args = argc>1 ? (const char**)malloc((argc-1) * sizeof(char*)) : NULL;
	for(int i=1; i<argc; ++i) {
		py_str(py_arg(i));
		args[i-1] = py_tostr(py_retval());
	}

	char* script = ResourceGetText(fname);
	if(!script)
		return ImportError("window.switchScene(%s): file not found", fname);

	// call leave event on current script
	dispatchLifecycleEvent("leave", NULL);
	// clear current callbacks
	py_setglobal(py_name("enter"), py_NIL());
	py_setglobal(py_name("input"), py_NIL());
	py_setglobal(py_name("update"), py_NIL());
	py_setglobal(py_name("draw"), py_NIL());
	py_setglobal(py_name("leave"), py_NIL());

	bool ok = py_exec(script, fname, EXEC_MODE, NULL);
	free(script);
	if(!ok || py_checkexc(false)) {
		free(args);
		return handleException();
	}
	dispatchLifecycleEventArgv("enter", argc-1, (char**)args, NULL);
	free(args);

	py_newnone(py_retval());
	return true;
}

// --- gfx bindings ---
static bool py_gfxColor(int argc, py_StackRef argv) {
	int64_t color;
	if(!py_castint(py_arg(0), &color))
		return false;

	gfxColor((uint32_t)color);
	py_newnone(py_retval());
	return true;
}

static bool py_gfxLineWidth(int argc, py_StackRef argv) {
	float w;
	if(!py_castfloat32(py_arg(0), &w))
		return false;

	gfxLineWidth(w);
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

	gfxClipRect((int)x, (int)y, (int)w, (int)h);
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

	gfxDrawRect(x, y, w, h);
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

	gfxFillRect(x, y, w, h);
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

	gfxDrawLine(x0, y0, x1, y1);
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

	gfxDrawImage((uint32_t)img, x, y, rot, sc, (int)flip);
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

	//printf("gfxFillTextAlign(%ld, %f, %f, \"%s\", %d)\n", font, x, y, str, (int)align);
	gfxFillTextAlign((uint32_t)font, x, y, str, (int)align);
	py_newnone(py_retval());
	return true;
}

// --- audio bindings ---
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

	uint32_t track = AudioReplay((uint32_t)sample, vol, bal, det);
	if(track == UINT32_MAX)
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

	arcmAudioVolume((uint32_t)track, volume, fadeTime);
	py_newnone(py_retval());
	return true;
}

// --- resource bindings ---
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

	uint32_t handle = arcmResourceGetImage(name, scale, centerX, centerY, filtering);
	py_newint(py_retval(), (int64_t)handle);
	return true;
}

// binding for resource.createImage(array, width, height, centerX, centerY, filtering) to arcmResourceCreateImage(data, width, height, centerX, centerY, filtering)
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
	   !py_castint(py_arg(2), &height))
		return false;
	if(width*height < numItems) {
		free(data);
		return ValueError("resource.createImage() argument 1 expects width*height >= %zu\n", numItems);
	}
	if(argc > 3 && !py_castfloat32(py_arg(3), &centerX))
		return false;
	if(argc > 4 && !py_castfloat32(py_arg(4), &centerY))
		return false;
	if(argc > 5 && !py_castint(py_arg(5), &filtering))
		return false;

	uint32_t handle = arcmResourceCreateImage((uint8_t*)data, (int)width, (int)height, centerX, centerY, (int)filtering);
	free(data);
	py_newint(py_retval(), (int64_t)handle);
	return true;
}

// binding for resource.createSVGImage(svg, scale, centerX, centerY) to arcmResourceCreateSVGImage(svg, scale, centerX, centerY)
static bool py_ResourceCreateSVGImage(int argc, py_StackRef argv) {
	const char* svg = py_tostr(py_arg(0));
	float scale = 1.0f, centerX=0.0f, centerY=0.0f;
	if(argc > 1 && !py_castfloat32(py_arg(1), &scale))
		return false;
	if(argc > 2 && !py_castfloat32(py_arg(2), &centerX))
		return false;
	if(argc > 3 && !py_castfloat32(py_arg(3), &centerY))
		return false;

	uint32_t handle = arcmResourceCreateSVGImage(svg, scale, centerX, centerY);
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

	size_t handle = gfxImageTileGrid((uint32_t)img, (uint32_t)tilesX, (uint32_t)tilesY, (uint32_t)borderW);
	py_newint(py_retval(), (int64_t)handle);
	return true;
}

static bool py_ResourceGetAudio(int argc, py_StackRef argv) {
	const char* name = py_tostr(py_arg(0));
	size_t handle = ResourceGetAudio(name);
	py_newint(py_retval(), (int64_t)handle);
	return true;
}

// binding for resource.createAudio(list, numChannels) to AudioUploadPCM(waveData, numSamples, numChannels, 0)
static bool py_ResourceCreateAudio(int argc, py_StackRef argv) {
	// read data from an array/list containing float sample values:
	const size_t numSamples = py_islist(py_arg(0)) ? py_list_len(py_arg(0)) : 0;
	if(!numSamples)
		return TypeError("resource.createAudio() expects non-empty list containing numeric sample values as first argument");

	size_t numBytes = numSamples * sizeof(float);
	float* data = malloc(numBytes);
	for(size_t i=0; i<numSamples; ++i) {
		py_ItemRef item = py_list_getitem(py_arg(0), i);
		if(!py_castfloat32(item, &data[i]) || data[i] < -1.0f || data[i] > 1.0f) {
			free(data);
			return ValueError("resource.createAudio() argument 0 expects numeric sample value between -1.0 and 1.0 at position %i\n", (int64_t)i);
		}
	}

	int64_t numChannels = 1;
	if(argc > 1 && !py_castint(py_arg(1), &numChannels)) 
		return false;

	size_t handle = AudioUploadPCM(data, numSamples, numChannels, 0);
	py_newint(py_retval(), (int64_t)handle);
	return true;
}

static bool py_ResourceGetFont(int argc, py_StackRef argv) {
	PY_CHECK_ARGC(2);
	const char* name = py_tostr(py_arg(0));
	int64_t fontSize;
	if(!py_castint(py_arg(1), &fontSize))
		return false;

	size_t handle = ResourceGetFont(name, (uint32_t)fontSize);
	py_newint(py_retval(), (int64_t)handle);
	return true;
}

static bool py_ResourceGetStorageItem(int argc, py_StackRef argv) {
	PY_CHECK_ARGC(1);
	const char* key = py_tostr(py_arg(0));
	const char* val = arcmResourceGetStorageItem(key);
	if(!val) py_newnone(py_retval());
	else py_newstr(py_retval(), val);
	return true;
}

static bool py_ResourceSetStorageItem(int argc, py_StackRef argv) {
	PY_CHECK_ARGC(2);
	const char* key = py_tostr(py_arg(0));
	py_str(py_arg(1));
	const char* val = py_tostr(py_retval());
	arcmResourceSetStorageItem(key, val);
	py_newnone(py_retval());
	return true;
}

/// --- Bindings ---
static py_GlobalRef gfx_ns = NULL;

static void bindArcamini() {
	// unified arcamini namespace
	py_GlobalRef arcamini_ns = py_newmodule("arcamini");
	// window subnamespace

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
	py_bindfunc(resource_ns, "getTileGrid", py_ResourceGetTileGrid);
	py_bindfunc(resource_ns, "getAudio", py_ResourceGetAudio);
	py_bindfunc(resource_ns, "createAudio", py_ResourceCreateAudio);
	py_bindfunc(resource_ns, "getFont", py_ResourceGetFont);
	py_bindfunc(resource_ns, "getStorageItem", py_ResourceGetStorageItem);
	py_bindfunc(resource_ns, "setStorageItem", py_ResourceSetStorageItem);
	py_setdict(arcamini_ns, py_name("resource"), resource_ns);
}


/// --- VM Management ---

// Custom importfile callback
static char* custom_importfile(const char* module_name) {
	char* script = ResourceGetText(module_name);
	if (!script)
		fprintf(stderr, "Module not found: %s\n", module_name);
	return script;
}

void shutdownVM(void* context) {
	if(context)
		py_finalize();
}

// Creates and initializes a PocketPy VM and returns its context
void* initVM(const char* script, const char* scriptName) {
	py_initialize();
	py_callbacks()->importfile = custom_importfile;
	bindArcamini();
	void* ctx = (void*)1;

	// Evaluate user script
	bool ok = py_exec(script, scriptName, EXEC_MODE, NULL);
	if(!ok || py_checkexc(false)) {
		py_printexc();
		shutdownVM(ctx);
		return NULL;
	}
	return ctx;
}

/// --- Event dispatchers ---

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

void dispatchButtonEvent(size_t id, uint8_t button, float value, void* callback) {
	(void)callback;
	arcmWindowCloseOnButton67(id, button, value);

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

void dispatchDrawEvent(void* callback) {
	(void)callback;
	py_Ref fnDraw = py_getglobal(py_name("draw"));
	if(!fnDraw || (py_typeof(fnDraw) != tp_function && py_typeof(fnDraw) != tp_nativefunc))
		return;
	
	py_push(fnDraw);
	py_pushnil();
	py_push(gfx_ns);
	if(!py_vectorcall(1, 0))
		handleException();
}
