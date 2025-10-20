#!/usr/bin/env python3
# arcamini.py - CPython bindings for arcamini C library
import ctypes, struct
from ctypes import c_double, c_bool, c_float, c_uint, c_int, c_uint8
import sys, os, types
import importlib.abc, importlib.util

if __name__ == "__main__": # ensure arcamini is the main module when run as script or imported
    sys.modules["arcamini"] = sys.modules["__main__"]

# Load the shared library from the module directory:
if sys.platform == "darwin":
    _libname = "libarcamini.dylib"
elif sys.platform == "win32":
    _libname = "arcamini.dll"
elif sys.platform == "linux":
    _libname = f"libarcamini.{os.uname().machine}.so"
_lib = ctypes.CDLL(os.path.join(os.path.dirname(__file__), _libname))

# Expose isRunning global variable
_isRunning = ctypes.c_bool.in_dll(_lib, "isRunning")


# Callback prototypes
INPUT_CB  = ctypes.CFUNCTYPE(None, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, c_float, c_float)
UPDATE_CB = ctypes.CFUNCTYPE(c_bool, c_double)
DRAW_CB   = ctypes.CFUNCTYPE(None)
# Keep refs
_cb_refs = {}
cbInput, cbUpdate, cbDraw, cbLeave = None, None, None, None

# Register functions
_lib.arcamini_set_callbacks.argtypes = [INPUT_CB, UPDATE_CB, DRAW_CB]
_lib.arcamini_init.argtypes   = [ctypes.c_int, ctypes.c_int, c_bool, ctypes.c_char_p, ctypes.c_char_p]
_lib.arcamini_init.restype    = c_bool
_lib.arcamini_run.restype     = None
#char* ResourceGetText(const char* name);
_lib.ResourceGetText.argtypes = [ctypes.c_char_p]
_lib.ResourceGetText.restype  = ctypes.c_char_p

#--- window API ---
window = types.SimpleNamespace()
window._width = 0
window._height = 0
window.width = lambda: window._width
window.height = lambda: window._height
window.color = lambda color: _lib.WindowClearColor(c_uint(color))

#extern int WindowWidth();
_lib.WindowWidth.argtypes = []
_lib.WindowWidth.restype = c_int
#extern int WindowHeight();
_lib.WindowHeight.argtypes = []
_lib.WindowHeight.restype = c_int
#extern void WindowClearColor(uint32_t color);
_lib.WindowClearColor.argtypes = [c_uint]
_lib.WindowClearColor.restype = None

def _switchScene(fname, *args):
    """Switch to another script/scene, passing optional string arguments"""
    global cbInput, cbUpdate, cbDraw, cbLeave

    if cbLeave:
        try:
            cbLeave()
        except Exception:
            import traceback
            traceback.print_exc()
            _isRunning.value = False

    if not fname:
        _isRunning.value = False
        return

    if not fname.endswith(".py"):
        fname += ".py"
    script = _lib.ResourceGetText(fname.encode('utf-8'))
    if not script:
        print("Error: could not load script", fname)
        _isRunning.value = False
        return
    script = script.decode('utf-8')
    fname = fname[:-3]
    if fname in sys.modules:
        currScene = sys.modules[fname]
    else:
        import importlib.util
        spec = importlib.util.spec_from_loader(fname, loader=None)
        currScene = importlib.util.module_from_spec(spec)
        sys.modules[fname] = currScene
    exec(script, currScene.__dict__)

    cbInput = getattr(currScene, "input", None)
    cbUpdate = getattr(currScene, "update", None)
    cbDraw = getattr(currScene, "draw", None)
    cbLeave = getattr(currScene, "leave", None)
    cbEnter = getattr(currScene, "enter", None)
    if cbEnter:
        cbEnter(args if args else [])

window.switchScene = _switchScene

#--- audio API ---
audio = types.SimpleNamespace()
#extern uint32_t AudioReplay(uint32_t sample, float volume, float balance, float detune);
_lib.AudioReplay.argtypes = [c_uint, c_float, c_float, c_float]
_lib.AudioReplay.restype = c_uint
audio.replay = lambda sample, volume=1.0, balance=0.0, detune=0.0: _lib.AudioReplay(
    c_uint(sample), c_float(volume), c_float(balance), c_float(detune))
#extern void arcmAudioVolume(uint32_t track, float volume, float fadeTime);
_lib.arcmAudioVolume.argtypes = [c_uint, c_float, c_float]
_lib.arcmAudioVolume.restype = None
audio.volume = lambda track, volume=1.0, fadeTime=0.0: _lib.arcmAudioVolume(
    c_uint(track), c_float(volume), c_float(fadeTime))

#--- resource API ---
resource = types.SimpleNamespace()
#extern uint32_t arcmResourceGetImage(const char* name, float scale, float centerX, float centerY, int filtering);
_lib.arcmResourceGetImage.argtypes = [ctypes.c_char_p, c_float, c_float, c_float, c_int]
_lib.arcmResourceGetImage.restype = c_uint
resource.getImage = lambda name, scale=1.0, centerX=0.0, centerY=0.0, filtering=1: _lib.arcmResourceGetImage(
    name.encode('utf-8'), c_float(scale), c_float(centerX), c_float(centerY), c_int(filtering))

#extern uint32_t arcmResourceCreateImage(const uint8_t* data, int width, int height, float centerX, float centerY, int filtering);
_lib.arcmResourceCreateImage.argtypes = [ctypes.POINTER(c_uint), c_int, c_int, c_float, c_float, c_int]
_lib.arcmResourceCreateImage.restype = c_uint
resource.createImage = lambda data, width, height, centerX=0.0, centerY=0.0, filtering=1: _lib.arcmResourceCreateImage(
    (c_uint * len(data))(*data), c_int(width), c_int(height), c_float(centerX), c_float(centerY), c_int(filtering))

#extern uint32_t arcmResourceCreateSVGImage(const char* svg, float scale, float centerX, float centerY);
_lib.arcmResourceCreateSVGImage.argtypes = [ctypes.c_char_p, c_float, c_float, c_float]
_lib.arcmResourceCreateSVGImage.restype = c_uint
resource.createSVGImage = lambda svg, scale=1.0, centerX=0.0, centerY=0.0: _lib.arcmResourceCreateSVGImage(
    svg.encode('utf-8'), c_float(scale), c_float(centerX), c_float(centerY))

#extern uint32_t gfxImageTileGrid(uint32_t parent, uint16_t tilesX, uint16_t tilesY, uint16_t border);
_lib.gfxImageTileGrid.argtypes = [c_uint, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16]
_lib.gfxImageTileGrid.restype = c_uint
resource.getTileGrid = lambda parent, tilesX, tilesY=1, border=0: _lib.gfxImageTileGrid(
    c_uint(parent), ctypes.c_uint16(tilesX), ctypes.c_uint16(tilesY), ctypes.c_uint16(border))

#extern size_t ResourceGetAudio(const char* name);
_lib.ResourceGetAudio.argtypes = [ctypes.c_char_p]
_lib.ResourceGetAudio.restype = c_uint
resource.getAudio = lambda name: _lib.ResourceGetAudio(name.encode('utf-8'))

#uint32_t arcamini_createAudio(float* waveData, uint32_t numSamples, uint8_t numChannels)
_lib.arcamini_createAudio.argtypes = [ctypes.POINTER(c_float), c_uint, c_uint8]
_lib.arcamini_createAudio.restype = c_uint
resource.createAudio = lambda waveData, numChannels=1: _lib.arcamini_createAudio(
    (c_float * len(waveData))(*waveData), c_uint(len(waveData)), c_uint8(numChannels))

#extern size_t ResourceGetFont(const char* name, unsigned fontSize);
_lib.ResourceGetFont.argtypes = [ctypes.c_char_p, c_uint]
_lib.ResourceGetFont.restype = c_uint
resource.getFont = lambda name, fontSize=16: _lib.ResourceGetFont(name.encode('utf-8'), c_uint(fontSize))

#extern const char* arcmResourceGetStorageItem(const char* key);
_lib.arcmResourceGetStorageItem.argtypes = [ctypes.c_char_p]
_lib.arcmResourceGetStorageItem.restype = ctypes.c_char_p
resource.getStorageItem = lambda key: (_lib.arcmResourceGetStorageItem(key.encode('utf-8')) or b'').decode('utf-8')

#extern void arcmResourceSetStorageItem(const char* key, const char* value);
_lib.arcmResourceSetStorageItem.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
_lib.arcmResourceSetStorageItem.restype = None
resource.setStorageItem = lambda key, value: _lib.arcmResourceSetStorageItem(key.encode('utf-8'), value.encode('utf-8'))

# replace built-in breakpoint() by arcamini-compatible breakpoint(args)
import builtins
def breakpoint(*args):
    print("breakpoint", args)
    import pdb; pdb.set_trace()
builtins.breakpoint = breakpoint

# ensure modules are loaded from resources:
class ResourceLoader(importlib.abc.Loader):
    def __init__(self, code_bytes):
        self.code_bytes = code_bytes
    def create_module(self, spec):
        return None
    def exec_module(self, module):
        code_str = self.code_bytes.decode('utf-8')
        exec(code_str, module.__dict__)

class ResourceFinder(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path, target=None):
        fname = fullname + ".py"
        code_bytes = _lib.ResourceGetText(fname.encode('utf-8'))
        if code_bytes:
            return importlib.util.spec_from_loader(fullname, ResourceLoader(code_bytes))
        return None
sys.meta_path.append(ResourceFinder())

# Gfx API
_lib.gfxDrawBatch.argtypes = [
    ctypes.c_void_p, ctypes.c_uint,   # ops buffer + length
    ctypes.c_char_p, ctypes.c_uint    # string buffer + length
]
_lib.gfxDrawBatch.restype = None

# --- Opcodes (must match C enum) ---
OP_COLOR      = 1
OP_LINEWIDTH  = 2
OP_CLIPRECT   = 3
OP_FILLRECT   = 4
OP_DRAWRECT   = 5
OP_DRAWLINE   = 6
OP_DRAWIMAGE  = 7
OP_FILLTEXT   = 8

class Gfx:
    """arcamini graphics context"""
    def __init__(self):
        self.ops = bytearray()
        self.strings = bytearray()
        self.string_offsets = {}  # cache for reused strings

    def _emit(self, fmt, opcode, *args):
        """Pack one drawing op into the ops buffer"""
        self.ops += struct.pack("<I" + fmt, opcode, *args)

    def _emit_string(self, s: str) -> int:
        """Store string in string table, return offset"""
        if s in self.string_offsets:
            return self.string_offsets[s]
        offset = len(self.strings)
        data = s.encode("utf-8") + b"\0"   # null-terminated
        self.strings += data
        self.string_offsets[s] = offset
        return offset

    def color(self, clr):
        self._emit("I", OP_COLOR, clr)

    def lineWidth(self, w):
        self._emit("f", OP_LINEWIDTH, w)

    def clipRect(self, x, y, w, h):
        self._emit("iiii", OP_CLIPRECT, x, y, w, h)

    def fillRect(self, x, y, w, h):
        self._emit("ffff", OP_FILLRECT, x, y, w, h)
    
    def drawRect(self, x, y, w, h):
        self._emit("ffff", OP_DRAWRECT, x, y, w, h)

    def drawLine(self, x1, y1, x2, y2):
        self._emit("ffff", OP_DRAWLINE, x1, y1, x2, y2)

    def drawImage(self, image, x, y, rot=0.0, sc=1.0, flip=0):
        self._emit("Iffffi", OP_DRAWIMAGE, image, x, y, rot, sc, flip)

    def fillText(self, font, x, y, string: str, align=0):
        self._emit("IffII", OP_FILLTEXT, font, x, y, self._emit_string(str(string)), align)

    def flush(self):
        """Flush any pending drawing operations"""
        if not self.ops:
            return
        ops_buf = (ctypes.c_ubyte * len(self.ops)).from_buffer(self.ops)
        str_buf = (ctypes.c_char * len(self.strings)).from_buffer(self.strings) if self.strings else None
        _lib.gfxDrawBatch(ops_buf, len(self.ops), str_buf, len(self.strings))
        del ops_buf, str_buf
        self.ops.clear()
        self.strings.clear()
        self.string_offsets.clear()

def init(width, height, fullscreen, fname:str):
    """Initialize the arcamini system"""
    archiveName = os.path.dirname(fname)
    if not archiveName:
        archiveName = os.path.curdir
    scriptBaseName = os.path.basename(fname).replace("_", " ")
    if scriptBaseName.endswith(".py"):
        scriptBaseName = scriptBaseName[:-3]

    if not _lib.arcamini_init(width, height, fullscreen,
        archiveName.encode('utf-8'), scriptBaseName.encode('utf-8')):
        raise RuntimeError("Failed to initialize arcamini with startup script " + fname)
    window._width = _lib.WindowWidth()
    window._height = _lib.WindowHeight()

def run():
    """ register C callbacks and run main loop"""
    global cbInput, cbUpdate, cbDraw, cbLeave
    gfx = Gfx()
    
    def _input(evt, device, id, value, value2):
        if not cbInput:
            return
        try:
            cbInput(evt.decode('utf-8'), device, id, value, value2)
        except Exception:
            import traceback
            traceback.print_exc()
            _isRunning.value = False

    def _update(dt):
        if not cbUpdate:
            return False
        try:
            return bool(cbUpdate(dt))
        except Exception:
            import traceback
            traceback.print_exc()
            _isRunning.value = False

    def _draw():
        if not cbDraw:
            return
        try:
            cbDraw(gfx)
            gfx.flush()
        except Exception:
            import traceback
            traceback.print_exc()
            _isRunning.value = False

    inp_cb = INPUT_CB(_input)
    up_cb = UPDATE_CB(_update)
    dr_cb = DRAW_CB(_draw)

    _cb_refs["input"] = inp_cb
    _cb_refs["update"] = up_cb
    _cb_refs["draw"] = dr_cb

    _lib.arcamini_set_callbacks(inp_cb, up_cb, dr_cb)
    _lib.arcamini_run()
    if cbLeave:
        try:
            cbLeave()
        except Exception:
            import traceback
            traceback.print_exc()
            _isRunning.value = False
        cbLeave = None


if __name__ != "__main__" or len(sys.argv) < 2:
    print("Usage: python3 -m arcamini [-f(ullscreen) -w width -h height] <script> [args...]")
    sys.exit(1)

window_width, window_height, window_fullscreen = 640, 480, False
if '-w' in sys.argv and sys.argv.index('-w') + 1 < len(sys.argv):
    window_width = int(sys.argv[sys.argv.index('-w') + 1])
    sys.argv.remove(sys.argv[sys.argv.index('-w') + 1])
    sys.argv.remove('-w')
if '-h' in sys.argv and sys.argv.index('-h') + 1 < len(sys.argv):
    window_height = int(sys.argv[sys.argv.index('-h') + 1])
    sys.argv.remove(sys.argv[sys.argv.index('-h') + 1])
    sys.argv.remove('-h')
if '-f' in sys.argv:
    window_fullscreen = True
    sys.argv.remove('-f')

fname = sys.argv[1]

init(window_width, window_height, window_fullscreen, fname)
_switchScene(os.path.basename(fname), *sys.argv[2:])
run()
