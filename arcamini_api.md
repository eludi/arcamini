# arcamini API

## module global

extensions to the global namespace
### function breakpoint
breaks if a debugger is attached and passes its argument list to it
#### Parameters:
- {any} args
- {any} ...

#### Returns:
- {uint32}

### callback function load
Called after the script has been loaded before entering the main loop

### callback function input
Called when a user input event occurs
#### Parameters:
- {string} evt
- {int} device
- {int} id
- {float} value
- {float} value2

### callback function update
Called each frame before draw to update the game state. Return true to keep the main loop running.
#### Parameters:
- {double} deltaT

#### Returns:
- {bool}

### callback function draw
Called to render a frame after update
#### Parameters:
- {gfx} gfx

### callback function unload
Called before the script is unloaded after exiting the main loop

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
- {uint32} color

## module gfx

2D graphics context passed to the draw() callback
### function color
#### Parameters:
- {uint32} color

### function lineWidth
#### Parameters:
- {float} w

### function fillRect
#### Parameters:
- {float} x
- {float} y
- {float} w
- {float} h

### function drawRect
#### Parameters:
- {float} x
- {float} y
- {float} w
- {float} h

### function drawLine
#### Parameters:
- {float} x0
- {float} y0
- {float} x1
- {float} x2

### function drawImage
#### Parameters:
- {uint32} image
- {float} x
- {float} y
- {float} rot (default: 0.0)
- {float} sc (default: 1.0)
- {int} flip (default: 0)

### function fillText
#### Parameters:
- {uint32} font
- {float} x
- {float} y
- {string} str
- {int} align (default: 0)

## module audio

audio playback functions
### function replay
immediately plays a sample identified by its handle
#### Parameters:
- {uint32} sample
- {float} volume (default: 1.0)
- {float} balance (default: 0.0)
- {float} detune (default: 0.0)

#### Returns:
- {uint32}

## module resource

functions to load and create images, audio samples and fonts
### function getImage
#### Parameters:
- {string} name
- {float} scale (default: 1.0)
- {float} centerX (default: 0.0)
- {float} centerY (default: 0.0)
- {int} filtering (default: 1)

#### Returns:
- {uint32}

### function createImage
#### Parameters:
- {array<uint32>} data - image data as array of uint32 RGBA pixels, or as Uint32Array (Javascript only)
- {int} width
- {int} height
- {float} centerX (default: 0.0)
- {float} centerY (default: 0.0)
- {int} filtering (default: 1)

#### Returns:
- {uint32}

### function createSVGImage
#### Parameters:
- {string} svg
- {float} scale (default: 1.0)
- {float} centerX (default: 0.0)
- {float} centerY (default: 0.0)

#### Returns:
- {uint32}

### function getTileGrid
#### Parameters:
- {uint32} image
- {int} tilesX
- {int} tilesY (default: 1)
- {int} borderW (default: 0)

#### Returns:
- {uint32}

### function getAudio
#### Parameters:
- {string} name

#### Returns:
- {uint32}

### function createAudio
#### Parameters:
- {array<float>} data - PCM audio samples either passed as array of floats [-1.0, 1.0], or as Float32Array (Javascript only)
- {int} numChannels (default: 1)

#### Returns:
- {uint32}

### function getFont
#### Parameters:
- {string} name
- {uint32} fontSize (default: 16)

#### Returns:
- {uint32}

### function setStorageItem
sets a value in an app-specific persistent key-value store
#### Parameters:
- {string} key
- {string} value

### function getStorageItem
retrieves a value from an app-specific persistent key-value store. Returns NULL if the key does not exist
#### Parameters:
- {string} key

#### Returns:
- {string}
