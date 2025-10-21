img = resource.getImage("test.png")
rings = resource.getTileGrid(resource.getImage("rings.svg", 1.0, 0.5, 0.5), 5)
font = resource.getFont("Viafont.ttf", 32)
sample = resource.getAudio("ding.wav")
function createBeep(freq, dur, vol)
    vol = vol or 1.0
    local data = {}
    for n = 0, math.floor(44100 * dur) - 1 do
        data[n+1] = math.sin(2 * math.pi * freq * n / 44100) * vol
    end
    return resource.createAudio(data)
end
tone880 = createBeep(880, 1.0)

resource.setStorageItem("key", "value")
local val = resource.getStorageItem("key")
print("Storage value:", val)

frame = 0

function enter(args)
    print("enter called, args:", args)
    print("window dimensions:", window.width(), window.height())
    window.color(0x000055ff)
    audio.replay(sample, 1, 0.5)
    local track = audio.replay(tone880, 0.5, -0.5)
    audio.volume(track, 0.0, 1.0) -- fade out over 1 second
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
    gfx.clipRect(50+frame%224,200,32,256)
    gfx.drawImage(img, 50, 200)
    gfx.clipRect(0,0,-1,-1)

    gfx.save()
    local tile = (frame // 6) % 5
    gfx.color(0xFFFFFFFF - 0x333300*tile)
    gfx.transform(window.width()-32, window.height()-32, 0, 2)
    gfx.drawImage(rings + tile, 0,0)
    gfx.restore()

    if frame < 2 then
        print("draw called at frame", frame)
    end
    frame = frame + 1
end

function leave()
    print("leave called")
end
