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

