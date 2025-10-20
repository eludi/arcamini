from arcamini import resource, window, audio
import math

img = resource.getImage("test.png")
font = resource.getFont("Viafont.ttf", 32)
sample = resource.getAudio("ding.wav")
def createBeep(freq, dur, vol=1.0):
    return resource.createAudio([math.sin(2 * math.pi * freq * n / 44100) * vol
        for n in range(int(44100 * dur))])
tone880 = createBeep(880, 1.0)

resource.setStorageItem("key", "value")
val = resource.getStorageItem("key")
print("Storage value:", val)

frame = 0

# window module
def enter(args):
    print("enter called, args:", args)
    print("window dimensions:", window.width(), window.height())
    window.color(0x000055ff)
    audio.replay(sample, 1, 0.5)
    track = audio.replay(tone880, 0.5, -0.5)
    audio.volume(track, 0.0, 1.0) # fade out over 1 second

def input(evt, device, id, value, value2):
    print(f"input({evt}, {device}, {id}, {value}, {value2})")

def update(deltaT):
    global frame
    if frame < 2:
        print(f"update called with deltaT {deltaT} at frame {frame}")
    return True

def draw(gfx):
    global frame
    gfx.color(0xFF0000FF)
    gfx.lineWidth(2.0)
    gfx.fillRect(120, 10, 100, 50)
    gfx.drawRect(230, 10, 100, 50)
    gfx.drawLine(0, 0, 100, 100)
    gfx.fillText(font, 20, 120, "Hello", 0)
    gfx.color(0xFFffFFff)
    gfx.clipRect(50+frame%224,200,32,256)
    gfx.drawImage(img, 50, 200)
    gfx.clipRect(0,0,-1,-1)
    if frame < 2:
        print("draw called at frame", frame)
    frame += 1

def leave():
    print("leave called")
