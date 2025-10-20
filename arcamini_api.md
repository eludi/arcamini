# arcamini API

## module global

extensions to the global namespace
### function breakpoint
breaks if a debugger is attached and passes its argument list to it. Passed arguments are available as array args in the debug session.
#### Parameters:
- {any} args
- {any} ...

### callback function enter
Called when entering a script/scene, either on initial load or after a call to switchScene()
#### Parameters:
- {array<string>} args - the arguments passed to switchScene() or as command line parameters after the script name if the scene was loaded at startup

### callback function input
Called when an input event occurs
#### Parameters:
- {string} evt - the input event type name. Currently 'button' or 'axis'.
- {int} device - the input device ID. 0 for primary device.
- {int} id - axis or button id, starting with 0
- {float} value - the input event value, 0.0 or 1.0 for buttons, or a value between -1.0 and 1.0 for axes
- {float} value2 - the second input event value, if applicable. Currently unused.

### callback function update
Called each frame before draw to update the game state. Return true to keep the main loop running.
#### Parameters:
- {double} deltaT - time in seconds since the last call to update()

#### Returns:
- {bool}

### callback function draw
Called to render a frame after update
#### Parameters:
- {gfx} gfx - the graphics context to call draw functions on

### callback function leave
Called when leaving a scene before switching to another scene or shutting down the application, either by returning false from update(), by closing the application window, or by pressing buttons 6 and 7 together.

## module window

functions to query and manipulate the application window
### function width
Returns the current window width in pixels

#### Returns:
- {int}

### function height
Returns the current window height in pixels

#### Returns:
- {int}

### function color
Sets the window background color
#### Parameters:
- {uint32} color - window background color. Usually a hex value like 0xRRGGBBAA.

### function switchScene
Switches to another script as event handler. Calls leave() on the current scene before switching and enter(args) on the new scene. This is useful for organizing an app/game in separate scenes or screens.
#### Parameters:
- {string} script - the file name of the script that takes over the event handling
- {any} args - the argument to be passed to the new script. Will be converted to string.
- {any} ... - optional additional arguments to be passed to the new script

## module gfx

2D graphics context passed to the draw() callback
### function color
Sets the draw color
#### Parameters:
- {uint32} color - the draw color. Usually a hex value like 0xRRGGBBAA.

### function lineWidth
Sets the line width for drawLine and drawRect
#### Parameters:
- {float} w - the line width. Default is 1.0 pixels

### function clipRect
Sets the clipping viewport for all subsequent draw calls in screen space. Default is (0,0,width,height) of the window. Use negative width/height to disable clipping.
#### Parameters:
- {int} x - the horizontal position of the clipping rectangle's upper left corner
- {int} y - the vertical position of the clipping rectangle's upper left corner
- {int} w - the width of the clipping rectangle
- {int} h - the height of the clipping rectangle

### function fillRect
Draws a filled rectangle at position (x,y) with width w and height h
#### Parameters:
- {float} x - the horizontal position of the rectangle's upper left corner
- {float} y - the vertical position of the rectangle's upper left corner
- {float} w - the width of the rectangle
- {float} h - the height of the rectangle

### function drawRect
Draws the outline of a rectangle at position (x,y) with width w and height h
#### Parameters:
- {float} x - the horizontal position of the rectangle's upper left corner
- {float} y - the vertical position of the rectangle's upper left corner
- {float} w - the width of the rectangle
- {float} h - the height of the rectangle

### function drawLine
Draws a line from (x0,y0) to (x1,y1)
#### Parameters:
- {float} x0 - the horizontal position of the line's start point
- {float} y0 - the vertical position of the line's start point
- {float} x1 - the horizontal position of the line's end point
- {float} y1 - the vertical position of the line's end point

### function drawImage
Draws an image
#### Parameters:
- {uint32} image - the image resource handle
- {float} x - the horizontal position of the image
- {float} y - the vertical position of the image
- {float} rot (default: 0.0) - the rotation angle of the image
- {float} sc (default: 1.0) - the scale factor of the image
- {int} flip (default: 0) - the flip mode of the image. 0 = no flip, 1 = horizontal flip, 2 = vertical flip, 3 = both flip

### function fillText
Draws filled text
#### Parameters:
- {uint32} font - the font resource handle. Use 0 for built-in 12x16 font
- {float} x - the horizontal position of the text
- {float} y - the vertical position of the text
- {string} str - the text string to draw
- {int} align (default: 0) - the text alignment. 0 = left, 1 = center, 2 = right

## module audio

audio playback functions
### function replay
immediately plays a sample identified by its handle. Returns a track handle that can be used to manipulate the playback
#### Parameters:
- {uint32} sample - the audio sample resource handle
- {float} volume (default: 1.0) - the volume level to play the sample at
- {float} balance (default: 0.0) - the stereo balance of the sample in the range [-1.0, 1.0]
- {float} detune (default: 0.0) - the detune amount in halftones

#### Returns:
- {uint32}

### function volume
sets the volume level of a currently playing track. Set to 0.0 to stop the track.
#### Parameters:
- {uint32} track - the track handle returned by the replay function
- {float} volume - the new volume level for the track in the range [0.0, 1.0]
- {float} fadeTime (default: 0.0) - the time in seconds to fade to 0. Other volume levels are not supported yet.

## module resource

functions to load and create images, audio samples and fonts
### function getImage
loads an image from the app's directory. Supported image formats are PNG, JPEG, and SVG. Returns image handle or 0 if the image could not be found or loaded
#### Parameters:
- {string} name - the image file name relative to the app's root directory
- {float} scale (default: 1.0) - the scale factor of the image. Only relevant for SVG vector images
- {float} centerX (default: 0.0) - the relative horizontal center position of the image in range [0.0, 1.0]
- {float} centerY (default: 0.0) - the relative vertical center position of the image in range [0.0, 1.0]
- {int} filtering (default: 1) - the image filtering mode. 0 = nearest neighbor, 1 = bilinear

#### Returns:
- {uint32}

### function createImage
creates an image from raw pixel data. Returns image handle or 0 if the image could not be created
#### Parameters:
- {array<uint32>} data - image data as array of uint32 RGBA pixels, or as Uint32Array (Javascript only)
- {int} width - the image width in pixels
- {int} height - the image height in pixels
- {float} centerX (default: 0.0) - the relative horizontal center position of the image in range [0.0, 1.0]
- {float} centerY (default: 0.0) - the relative vertical center position of the image in range [0.0, 1.0]
- {int} filtering (default: 1) - the image filtering mode. 0 = nearest neighbor, 1 = bilinear

#### Returns:
- {uint32}

### function createSVGImage
creates an SVG image from an SVG string. Returns image handle or 0 if the image could not be created
#### Parameters:
- {string} svg - the SVG image data as a string
- {float} scale (default: 1.0) - the scale factor of the image
- {float} centerX (default: 0.0) - the relative horizontal center position of the image in range [0.0, 1.0]
- {float} centerY (default: 0.0) - the relative vertical center position of the image in range [0.0, 1.0]

#### Returns:
- {uint32}

### function getTileGrid
creates multiple tile images an existing image. The image is divided into tilesX x tilesY tiles, optionally with a border of borderW pixels between the tiles. Returns handle of the first tile or 0 if the tile images could not be created.
#### Parameters:
- {uint32} image - the handle of the parent image
- {int} tilesX - the number of tiles in horizontal direction
- {int} tilesY (default: 1) - the number of tiles in vertical direction
- {int} borderW (default: 0) - the width of the border around tiles in pixels

#### Returns:
- {uint32}

### function getAudio
loads an audio sample from the app's directory. Returns audio sample handle or 0 if the sample could not be found or loaded
#### Parameters:
- {string} name - the audio file name relative to the app's root directory

#### Returns:
- {uint32}

### function createAudio
creates an audio sample from raw PCM data. Returns audio sample handle or 0 if the sample could not be created
#### Parameters:
- {array<float>} data - PCM audio samples either passed as array of floats [-1.0, 1.0], or as Float32Array (Javascript only)
- {int} numChannels (default: 1) - the number of audio channels (1 = mono, 2 = stereo)

#### Returns:
- {uint32}

### function getFont
loads a font from the app's directory at the specified size. Supported font formats are TTF or image formats. In the latter case, it is assumed that the image contains a grid of 16x16 glyphs. Returns font handle or 0 if the font could not be found or loaded.
#### Parameters:
- {string} name - the font file name
- {uint32} fontSize (default: 16) - the font size

#### Returns:
- {uint32}

### function setStorageItem
sets a value in an app-specific persistent key-value store
#### Parameters:
- {string} key - the key name
- {string} value - the value to store

### function getStorageItem
retrieves a value from an app-specific persistent key-value store. Returns null if the key does not exist.
#### Parameters:
- {string} key - the key name

#### Returns:
- {string}
