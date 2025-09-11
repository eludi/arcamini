#pragma once
/// arcajs minimal subset C API
#include <stdint.h>
#include <stddef.h>

///@{ \module window
/// sets window title
extern void WindowTitle(const char *str);
/// returns window width
extern int WindowWidth();
/// returns window height
extern int WindowHeight();
/// sets clear color
/** exposed as window.color(color) */
extern void WindowClearColor(uint32_t color);
///@}

///@{ \module gfx
/// sets current color
extern void gfxColor(uint32_t color);
/// sets current line width
extern void gfxLineWidth(float w);
/// draws a rectangle outline
extern void gfxDrawRect(float x, float y, float w, float h);
/// draws a filled rectangle
extern void gfxFillRect(float x, float y, float w, float h);
/// draws a line
extern void gfxDrawLine(float x0, float y0, float x1, float y1);
/// draws an image
extern void gfxDrawImage(uint32_t img, float x, float y, float rot, float sc, int flip);
/// @brief renders text
/** exposed as gfx.fillText(font, x, y, string[, align=0]) */
extern void gfxFillTextAlign(uint32_t font, float x, float y, const char* str, int align);
///@}

///@{ \module audio
/// immediately plays previously uploaded sample data
/** \note For stereo samples, detune and balance must be 0.0f
 * \return track number playing this sound or UINT_MAX if the input is invalid or no track is available */
extern uint32_t AudioReplay(uint32_t sample, float volume, float balance, float detune);
///@}

///@{ \module resource
/// returns handle to an image resource
/** @param scale only relevant for SVG images
    exposed as resource.getImage(name[, scale=1.0, centerX=0.0, centerY=0.0, filtering=1]) */
extern uint32_t arcmResourceGetImage(const char* name, float scale, float centerX, float centerY, int filtering);
/// returns handle to an RGBA image resource created from a buffer or array
extern uint32_t arcmResourceCreateImage(const uint8_t* data, int width, int height, float centerX, float centerY, int filtering);
/// returns handle to an image resource created from an SVG string
extern uint32_t arcmResourceCreateSVGImage(const char* svg, float scale, float centerX, float centerY);
/// defines a regular grid of image resources based on a parent image resource
/** @return handle of first subimage resource
 * exposed as resource.imageTileGrid(parent, tilesX[, tilesY=1, border=0]) */
extern uint32_t gfxImageTileGrid(uint32_t parent, uint16_t tilesX, uint16_t tilesY, uint16_t border);
/// returns handle to an audio resource
extern size_t ResourceGetAudio(const char* name);
/// uploads mono or stereo PCM wave data and returns a handle for later playback
/** exposed as resource.createAudio(waveData[, numChannels=1]) */
extern uint32_t AudioUploadPCM(float* waveData, uint32_t numSamples, uint8_t numChannels, uint32_t offset);
/// returns handle to a font resource
extern size_t ResourceGetFont(const char* name, unsigned fontSize);
/// returns pointer to a storage item value identified by a key
extern const char* arcmResourceGetStorageItem(const char* key);
/// sets a key-value pair storage item 
extern void arcmResourceSetStorageItem(const char* key, const char* value);

/// defines an image resource as section of a parent image. Returens handle of subimage resource.
//extern size_t arcmResourceGetTile(size_t parent, float x, float y, float w, float h, float centerX, float centerY);
/// returns an object containing text dimensions. Pass 0 as slen to determine length via strlen(str). 
//extern Value* arcmResourceQueryFont(size_t handle, const char* str, size_t slen);
/// returns an object containing an image's dimensions
//extern Value* arcmResourceQueryImage(size_t handle);
///@}

///@{ auxiliary functions mainly for the host application
extern void arcmStorageInit(const char* appName, const char* scriptBaseName);
extern void arcmStorageClose();
extern int arcmDispatchInputEvents(void* callback);
extern void arcmWindowCloseOnButton67(size_t id, uint8_t button, float value);
extern void WindowEmitClose();
extern void arcmShowError(const char* msg);
///@}
