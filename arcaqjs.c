#include "bindings.h"
#define QJS_DEBUG_IMPLEMENTATION
#include "qjs_debug.h"

#include "window.h"
#include "graphics.h"
#include "audio.h"
#include "resources.h"
#include "arcamini.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

int debug = 0;
static void onDebugSession(int evt) {
	if(evt == QJS_DEBUG_EVENT_QUIT)
		WindowClose();
	else
		WindowUpdateTimestamp();
}

#if defined __WIN32__ || defined WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

//------------------------------------------------------------------

int main(int argc, char** argv) {
	int winSzX = 640, winSzY = 480, windowFlags = WINDOW_VSYNC;
	char* archiveName = NULL;
	int debug_port = 0;
	if(argc<2 || strcmp(ResourceSuffix(argv[argc-1]),"js")!=0) {
		fprintf(stderr, "usage: %s [-w width] [-h height] [-f(ullscreen)] [-d debug_port] script.js\n", argv[0]);
		return 99;
	}
	for(int i=1; i<argc-1; ++i) {
		if(strcmp(argv[i],"-f")==0)
			windowFlags |= WINDOW_FULLSCREEN;
		else if(strcmp(argv[i],"-w")==0 && i+1<argc-1)
			winSzX = atoi(argv[++i]);
		else if(strcmp(argv[i],"-h")==0 && i+1<argc-1)
			winSzY = atoi(argv[++i]);
		else if(strcmp(argv[i],"-d")==0 && i+1<argc-1) {
			debug_port = atoi(argv[++i]);
			debug = 1;
		}
		else if(argv[i][0] == '-') {
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			return 99;
		}
	}

	char* scriptName = argv[argc-1];
	size_t pos = strlen(scriptName)-1;
	while(pos>0) {
		if(scriptName[pos]==PATHSEP)
			break;
		--pos;
	}
	if(!pos)
		archiveName=".";
	else {
		archiveName = scriptName;
		archiveName[pos] = 0;
		scriptName = &archiveName[pos+1];
	}

	if(!ResourceArchiveOpen(archiveName)) {
		fprintf(stderr, "Opening current working directory failed.\n");
		return -1;
	}
	char* script = ResourceGetText(scriptName);
	if (!script) {
		fprintf(stderr, "Opening script \"%s\" failed.\n", scriptName);
		return -1;
	}

	srand(time(NULL));
	AudioOpen(44100, 8);
	if(WindowOpen(winSzX, winSzY, windowFlags)!=0) {
		fprintf(stderr, "Setting video mode failed.\n");
		return -1;
	}
	winSzX = WindowWidth();
	winSzY = WindowHeight();
	gfxInit(winSzX, winSzY, 1, WindowRenderer());

	char* scriptBaseName = ResourceBaseName(scriptName);
	arcmStorageInit("arcaqjs", scriptBaseName);
	for(char* pch = scriptBaseName; *pch; ++pch)
		if(*pch=='_')
			*pch=' ';
	WindowTitle(scriptBaseName);
	free(scriptBaseName);
	WindowShowPointer(0);

	void* vm = initVM(script, scriptName);
	free(script);
	if(!vm) {
		fprintf(stderr, "evaluating \"%s\" failed.\n", scriptName);
		return -1;
	}
	qjs_debug_init(vm, debug_port, "breakpoint", onDebugSession);

	for(size_t i=0; i<WindowNumControllers(); ++i)
		WindowControllerOpen(i, 0);
	WindowEventHandler(arcmDispatchInputEvents, vm);

	if(dispatchLifecycleEvent("load", vm)) {
		while(WindowIsOpen()) {
			if (debug_port > 0)
				pkpy_debug_poll();
			if(!dispatchUpdateEvent(WindowDeltaT(), vm))
				break;

			gfxBeginFrame(WindowGetClearColor());
			dispatchDrawEvent(vm);
			gfxEndFrame();

			if(WindowUpdate()!=0)
				break;
		}
		dispatchLifecycleEvent("unload", vm);
	}

	if(debug) {
		printf("Cleaning up...");
		printf(" graphics..."); fflush(stdout);
	}
	gfxClose();
	if(debug) {
		printf(" window..."); fflush(stdout);
	}
	if(WindowIsOpen())
		WindowClose();

	if(debug) {
		printf(" vm..."); fflush(stdout);
	}
	qjs_debug_shutdown();
	shutdownVM(vm);
	ResourceArchiveClose();
	if(debug) {
		printf(" audio..."); fflush(stdout);
	}
	AudioClose();
	arcmStorageClose();
	if(debug) {
		printf(" READY.\n"); fflush(stdout);
	}
	return 0;
}
