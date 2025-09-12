#include "arcamini.h"

#include "graphics.h"
#include "resources.h"
#include "value.h"
#include "window.h"
#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//--- Resource -----------------------------------------------------
uint32_t arcmResourceGetImage(const char* name, float scale, float centerX, float centerY, int filtering) {
	//fprintf(stderr, "arcmResourceGetImage(%s, %f, %f, %f, %d)", name, scale, centerX, centerY, filtering);
    uint32_t handle = ResourceGetImage(name, scale, filtering);
    gfxImageSetCenter(handle, centerX, centerY);
    return handle;
}

uint32_t arcmResourceCreateImage(const uint8_t* data, int width, int height, float centerX, float centerY, int filtering) {
	uint32_t handle = ResourceCreateImage(width, height, data, filtering);
    gfxImageSetCenter(handle, centerX, centerY);
    return handle;
}

uint32_t arcmResourceCreateSVGImage(const char* svg, float scale, float centerX, float centerY) {
	uint32_t handle = ResourceCreateSVGImage(svg, scale);
    gfxImageSetCenter(handle, centerX, centerY);
    return handle;
}

//--- Storage ------------------------------------------------------
static char * storageFileName = NULL;
static Value* storageData = NULL;

static bool StorageLoad() {
	if(!storageData) {
		FILE* f = fopen(storageFileName, "r");
		if (!f)
			return false;
		// Seek to end to find size
		fseek(f, 0, SEEK_END);
		const long length = ftell(f);
		rewind(f);

		// Allocate memory (+1 for null terminator)
		char *buffer = (char *)malloc(length + 1);
		if (!buffer) {
			fprintf(stderr, "Memory allocation failed\n");
			fclose(f);
			return false;
		}
		// Read file into buffer
		const size_t read = fread(buffer, 1, length, f);
		buffer[read] = '\0';

		fclose(f);
		storageData = Value_parse(buffer);
		free(buffer);
	}
	return storageData ? true : false;
}

static bool StorageSave() {
	FILE* f = fopen(storageFileName, "w");
	if (!f) {
		fprintf(stderr, "localStorage file not writable\n");
		return false;
	}
	if(storageData)
		Value_print(storageData, f);
	return fclose(f) == 0;
}

void arcmStorageInit(const char* appName, const char* scriptBaseName) {
	const char* storagePath = SDL_GetPrefPath(appName, "arcaqjs");
	storageFileName = (char*)malloc(strlen(storagePath)+strlen(scriptBaseName)+6);
	storageFileName[0]=0;
	strcat(strcat(strcat(storageFileName, storagePath), scriptBaseName),".json");
	SDL_free((void*)storagePath);
	StorageLoad();
}
void arcmStorageClose() {
	free(storageFileName);
	free(storageData);
}

const char* arcmResourceGetStorageItem(const char* key) {
	return !storageData ? NULL : Value_gets(storageData, key, NULL);
}

void arcmResourceSetStorageItem(const char* key, const char* value) {
	if(!storageData && !StorageLoad())
		storageData = Value_new(VALUE_MAP, NULL);
	Value_sets(storageData, key, value);
	StorageSave();
}

//--- event handling -----------------------------------------------
void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* callback);
void dispatchButtonEvent(size_t id, uint8_t button, float value, void* callback);

int arcmDispatchInputEvents(void* callback) {
	const size_t numControllers = WindowNumControllers();
	SDL_Event evt;
	while( SDL_PollEvent( &evt )) switch(evt.type) {
		case SDL_KEYDOWN:
			if(!evt.key.repeat) switch(evt.key.keysym.sym) {
				case SDLK_LEFT: dispatchAxisEvent(numControllers+0, 0, -1.0f, callback); break;
				case SDLK_RIGHT: dispatchAxisEvent(numControllers+0, 0, +1.0f, callback); break;
				case SDLK_UP: dispatchAxisEvent(numControllers+0, 1, -1.0f, callback); break;
				case SDLK_DOWN: dispatchAxisEvent(numControllers+0, 1, 1.0f, callback); break;
				case SDLK_RETURN: dispatchButtonEvent(numControllers+0, 0, 1.0f, callback); break;
				case SDLK_BACKSPACE: dispatchButtonEvent(numControllers+0, 1, 1.0f, callback); break;
				case SDLK_RALT: dispatchButtonEvent(numControllers+0, 2, 1.0f, callback); break;
				case SDLK_RCTRL: dispatchButtonEvent(numControllers+0, 3, 1.0f, callback); break;
				case SDLK_TAB: dispatchButtonEvent(numControllers+0, 6, 1.0f, callback); break;
				case SDLK_ESCAPE: dispatchButtonEvent(numControllers+0, 7, 1.0f, callback); break;

				case 'a': dispatchAxisEvent(numControllers+1, 0, -1.0f, callback); break;
				case 'd': dispatchAxisEvent(numControllers+1, 0, 1.0f, callback); break;
				case 'w': dispatchAxisEvent(numControllers+1, 1, -1.0f, callback); break;
				case 's': dispatchAxisEvent(numControllers+1, 1, 1.0f, callback); break;
				case '1': dispatchButtonEvent(numControllers+1, 0, 1.0f, callback); break;
				case '2': dispatchButtonEvent(numControllers+1, 1, 1.0f, callback); break;
				case '3': dispatchButtonEvent(numControllers+1, 2, 1.0f, callback); break;
				case '4': dispatchButtonEvent(numControllers+1, 3, 1.0f, callback); break;
			}
			break;
		case SDL_KEYUP: {
				if(evt.type == SDL_KEYUP) switch(evt.key.keysym.sym) {
				case SDLK_LEFT:
				case SDLK_RIGHT: dispatchAxisEvent(numControllers+0, 0, 0.0f, callback); break;
				case SDLK_UP:
				case SDLK_DOWN: dispatchAxisEvent(numControllers+0, 1, 0.0f, callback); break;
				case SDLK_RETURN: dispatchButtonEvent(numControllers+0, 0, 0.0f, callback); break;
				case SDLK_BACKSPACE: dispatchButtonEvent(numControllers+0, 1, 0.0f, callback); break;
				case SDLK_RALT: dispatchButtonEvent(numControllers+0, 2, 0.0f, callback); break;
				case SDLK_RCTRL: dispatchButtonEvent(numControllers+0, 3, 0.0f, callback); break;
				case SDLK_TAB: dispatchButtonEvent(numControllers+0, 6, 0.0f, callback); break;
				case SDLK_ESCAPE: dispatchButtonEvent(numControllers+0, 7, 0.0f, callback); break;

				case 'a':
				case 'd': dispatchAxisEvent(numControllers+1, 0, 0.0f, callback); break;
				case 'w':
				case 's': dispatchAxisEvent(numControllers+1, 1, 0.0f, callback); break;
				case '1': dispatchButtonEvent(numControllers+1, 0, 0.0f, callback); break;
				case '2': dispatchButtonEvent(numControllers+1, 1, 0.0f, callback); break;
				case '3': dispatchButtonEvent(numControllers+1, 2, 0.0f, callback); break;
				case '4': dispatchButtonEvent(numControllers+1, 3, 0.0f, callback); break;
			}
			break;
		}
		case SDL_QUIT:
			return 1;
	}
	WindowControllerEvents(0.1, callback, dispatchAxisEvent, dispatchButtonEvent);
	return 0;
}

void arcmWindowCloseOnButton67(size_t id, uint8_t button, float value) {
	static uint16_t btnState[8] = {0};
	if(id<8 && button<16) {
		const uint16_t mask = (1 << button);
		if(value != 0.0f)
			btnState[id] |= mask;
		else
			btnState[id] &= ~mask;
		// close window if both buttons 6+7 (escape+tab) are pressed:
		if(btnState[id] & (1 << 6) && btnState[id] & (1 << 7))
			WindowEmitClose();
	}
}

void arcmShowError(const char* msg) {
	if(!WindowIsOpen()) {
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "arcamini ERROR", msg, NULL);
		return;
	}
	// split long messages into multiple lines
	char* lines[25];
	size_t numLines = 0;
	lines[numLines++] = (char*)msg;
	for(size_t i=0; i<numLines && numLines<25; ++i) {
		char* p = strchr(lines[i], '\n');
		if(p) {
			*p = 0;
			if(numLines<24)
				lines[numLines++] = p+1;
		}
	}

	uint32_t bg = ResourceCreateSVGImage("<svg width='240' height='170' version='1.1' xmlns='http://www.w3.org/2000/svg'><path style='fill:#ffffff' d='M 4,151 4,20 C 4,10.5 13,4 21,4 L 221,4 C 229.5,4 236,9 236,21 l 0,130 c 0,8.8 -6.8,15.5 -16,15.5 -14,0.5 -195.5,0 -199,0 -8.4,0 -16.5,-5 -16.3,-15 z M 219.8,20.8 H 21 V 151 H 220 Z M 75.3,133.5 c -2.3,-1.1 -3.5,-3 -3.8,-6.2 -0.4,-3.8 0.5,-5.4 4.7,-9.3 2.8,-2.6 9,-6.5 13.8,-8.9 22.7,-11 47.3,-9.4 68,4.4 9,6 11.3,8.8 11.3,13.3 0,8.7 -7.7,9.9 -17.2,2.6 -9.9,-7.5 -18,-10 -32,-10 -13.6,0 -22,2.6 -31.3,9.7 -7.7,6 -9.7,6.5 -13.7,4.5 z M 65,83 C 61.6,79.3 62.4,73.4 67,69 L 71.2,65 67,60 C 57,49 67.5,38.5 78.6,48.4 l 4.5,4 4.6,-4 c 5.3,-4.7 10,-5.2 13.4,-1.5 3.4,3.8 2.8,8 -1.8,13.3 l -4.2,4.7 4.2,4 c 4.8,4.6 5.3,9.3 1.4,13.3 -3.8,3.8 -6,3.5 -11.9,-1.5 l -5,-4.3 -5,4.3 c -5.4,4.8 -10.5,5.6 -13.9,2.1 z m 74.4,-0.4 c -3.4,-3.8 -2.8,-8 1.7,-13.2 l 4,-4.6 -4,-4.5 c -4.7,-5.2 -5.2,-10 -1.5,-13.4 3.8,-3.4 8,-2.9 13.2,1.7 l 4.5,4 4.6,-4 c 11.2,-9.8 21.4,0.5 11.6,11.7 l -4.2,4.7 4.2,4 c 4.8,4.6 5.5,10 1.6,13.8 -3.8,3.4 -8,2.8 -13,-1.7 l -4.6,-4 -4.5,4 c -5.2,4.6 -10,5 -13.4,1.5 z'/></svg>",
		WindowWidth()/1280.0f);
	gfxImageSetCenter(bg,0.5f,0.5f);
	WindowShowPointer(1);
	while(WindowIsOpen()) { // show error message until window is closed
		gfxBeginFrame(0x55002fff);
		gfxColor(0xffffff22);
		gfxDrawImage(bg, WindowWidth()/2, WindowHeight()*0.75f, 0.0f, 1.0f, 0);
		gfxColor(0xffffaaff);
		gfxFillText(0,12,20, "ERROR");
		gfxColor(0xddddddff);
		for(size_t i=0; i<numLines; ++i) {
			gfxFillText(0,12,60 + i*20, lines[i]);
		}
		gfxEndFrame();
		if(WindowUpdate()!=0)
			break;
	}
	//WindowEmitClose();
}
