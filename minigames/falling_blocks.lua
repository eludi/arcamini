local random = math.random
local blocks, score, highscore, state = {}, 0, 0, "play"
local winSzX, winSzY, sc = window.width(), window.height(), window.width() / 640
local player = { x = winSzX // 2, y = winSzY - 40 * sc, w = 40 * sc, h = 20 * sc, vx = 0 }

function beep(freq, dur, vol)
    vol = vol or 1.0
    local data = {}
    for n = 1, math.floor(44100 * dur) do
        data[n] = math.sin(2 * math.pi * freq * (n - 1) / 44100) * vol
    end
    return resource.createAudio(data)
end

local hit = beep(220, 0.2, 0.8)
local notes = {}
for _, f in ipairs({440, 494, 554, 659, 740, 880}) do
    table.insert(notes, beep(f, 0.05, 0.2))
end

function startup()
    window.color(0x202020ff)
    local h = resource.getStorageItem("highscore")
    highscore = h and tonumber(h) or 0
end

function shutdown()
    if score > highscore then highscore = score end
    resource.setStorageItem("highscore", tostring(highscore))
end

function input(evt, dev, id, val, v2)
    if state == "over" and evt == "button" and val > 0 then
        blocks = {}; score = 0; state = "play"
        player.x = winSzX // 2; player.vx = 0
    elseif state == "play" and evt == "axis" and id == 0 then
        player.vx = val * 200 * sc
    end
end

function update(dt)
    if state ~= "play" then return true end
    player.x = math.max(0, math.min(winSzX - player.w, player.x + player.vx * dt))
    if random() < 0.01 + score * 0.001 then
        table.insert(blocks, { random(0, winSzX - math.floor(20 * sc)), -math.floor(20 * sc) })
    end
    for i = #blocks, 1, -1 do
        local b = blocks[i]
        b[2] = b[2] + 150 * sc * dt
        if b[2] > winSzY then
            table.remove(blocks, i); score = score + 1
            audio.replay(notes[math.floor(b[1] / (winSzX / #notes)) + 1])
        elseif player.x < b[1] + 20 * sc and b[1] < player.x + player.w and
               player.y < b[2] + 20 * sc and b[2] < player.y + player.h then
            if score > highscore then highscore = score end
            state = "over"; audio.replay(hit)
        end
    end
    return true
end

function draw(gfx)
    gfx.color(0x00ff00ff); gfx.fillRect(player.x, player.y, player.w, player.h)
    gfx.color(0xff0000ff)
    for _, b in ipairs(blocks) do
        gfx.fillRect(b[1], b[2], 20 * sc, 20 * sc)
    end
    gfx.color(0xffffffaa)
    gfx.fillText(0, 10 * sc, 20 * sc, "Score: " .. score .. "  High: " .. highscore)
    if state == "over" then
        gfx.fillText(0, winSzX // 2, winSzY // 2, "GAME OVER - press any button", 1)
    end
end