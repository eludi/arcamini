import resource, window, audio

# Callback functions in global namespace
img = resource.getImage("test.png")
font = resource.getFont("Viafont.ttf", 32)
sample = resource.getAudio("beep.wav")
resource.setStorageItem("key", "value")
val = resource.getStorageItem("key")
print("Storage value:", val)

frame = 0

# window module
def load():
    print("load called")
    audio.replay(sample)
    print("window dimensions:", window.width(), window.height())
    window.color(0x000055ff)

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
    gfx.drawImage(img, 50, 200)
    if frame < 2:
        print("draw called at frame", frame)
    frame += 1

def unload():
    print("unload called")
