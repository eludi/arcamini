img = resource.getImage("test.png")
font = resource.getFont("Viafont.ttf", 32)
sample = resource.getAudio("beep.wav")
resource.setStorageItem("key", "value")
local val = resource.getStorageItem("key")
print("Storage value:", val)

local frame = 0

function load()
    print("load called")
    audio.replay(sample)
    print("window dimensions:", window.width(), window.height())
    window.color(0x000055ff)
end

function input(evt, device, id, value, value2)
    print(string.format("input(%s, %s, %s, %s, %s)", tostring(evt), tostring(device), tostring(id), tostring(value), tostring(value2)))
end

function update(deltaT)
    if frame < 2 then
        print(string.format("update called with deltaT %s at frame %d", tostring(deltaT), frame))
    end
    return true
end

function draw(gfx)
    gfx.color(0xFF0000FF)
    gfx.lineWidth(2.0)
    gfx.fillRect(120, 10, 100, 50)
    gfx.drawRect(230, 10, 100, 50)
    gfx.drawLine(0, 0, 100, 100)
    gfx.fillText(font, 20, 120, "Hello", 0)
    gfx.color(0xFFffFFff)
    gfx.drawImage(img, 50, 200)
    if frame < 2 then
        print("draw called at frame", frame)
    end
    frame = frame + 1
end

function unload()
    print("unload called")
end
