#include "bindings.h"
#define PKPY_DEBUG_IMPLEMENTATION
#include "pkpy_debug.h"

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
	if(evt == PKPY_DEBUG_EVENT_QUIT)
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
	const char* usage = "usage: %s [-w width] [-h height] [-f(ullscreen)] [-d debug_port] script.py [arg1, arg2, ...]\n";
	int argn;
	for(argn=1; argn<argc-1; ++argn) {
		if(strcmp(argv[argn],"-f")==0)
			windowFlags |= WINDOW_FULLSCREEN;
		else if(strcmp(argv[argn],"-w")==0 && argn+1<argc-1)
			winSzX = atoi(argv[++argn]);
		else if(strcmp(argv[argn],"-h")==0 && argn+1<argc-1)
			winSzY = atoi(argv[++argn]);
		else if(strcmp(argv[argn],"-d")==0 && argn+1<argc-1) {
			debug_port = atoi(argv[++argn]);
			debug = 1;
		}
		else if(argv[argn][0] == '-') {
			fprintf(stderr, "Unknown option: %s\n", argv[argn]);
			return 99;
		}
		else break;
	}

	char* scriptName = argn < argc ? argv[argn] : NULL;
	if(!scriptName || strcmp(ResourceSuffix(scriptName),"py")!=0) {
		fprintf(stderr, usage, argv[0]);
		return 99;
	}

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
	arcmStorageInit("arcaqpy", scriptBaseName);
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
	pkpy_debug_init(debug_port, "breakpoint", onDebugSession);

	for(size_t i=0; i<WindowNumControllers(); ++i)
		WindowControllerOpen(i, 0);
	WindowEventHandler(arcmDispatchInputEvents, vm);

	if(dispatchLifecycleEvent("startup", vm)) {
		dispatchLifecycleEventArgv("enter", argc-argn-1, argv+argn+1, vm);
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
		dispatchLifecycleEvent("leave", vm);
		dispatchLifecycleEvent("shutdown", vm);
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
	pkpy_debug_shutdown();
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
