local scene = {}
local bgColor = 0xFF4080FF -- Converted to Arcalua's color format
local winSzX, winSzY = window.width(), window.height()
local sc = math.min(winSzX / 640, winSzY / 480)
local title = resource.getImage("BALLATTAX.svg", sc, 0.5, 0.5)
assert(title > 0)
local font = resource.getFont("SpaceGrotesk-Bold.ttf", math.floor(32 * sc))
local selIndex = 0
local menu = { {label = "single player"}, {label = "multiplayer"}, {label = "exit"} }
local prevInputY = 0
local beeper = require("beeper")
local running = true

function scene.enter(args)
    for i, item in ipairs(menu) do
        item.x, item.y = winSzX / 2, winSzY / 2 + 40 * sc * i
    end
end

local function triggerMenuEvent(index)
    if index <=0 then
        return
    elseif index == 3 then
        running = false
        return
    end
    switchScene(require("game"), {players = index})
end

function scene.update(dt, inputs)
    if inputs[1].buttons[1] ~= 0 then
        triggerMenuEvent(selIndex)
    end
    if prevInputY == 0 and inputs[1].axes[2] ~= 0 then
        beeper:beep(inputs[1].axes[2] < 0 and 60 or 120, 0.01, 0.2)
        selIndex = selIndex + ((inputs[1].axes[2] > 0) and 1 or -1)
        if selIndex > #menu then
            selIndex = 1
        elseif selIndex < 1 then
            selIndex = #menu
        end
    end
    prevInputY = inputs[1].axes[2]
    return running
end

function scene.draw(gfx)
    gfx.color(bgColor)
    gfx.fillRect(0, 0, winSzX, winSzY)
    gfx.color(0xFFFFFFFF)
    gfx.drawImage(title, winSzX / 2, winSzY * 0.3)

    for i, item in ipairs(menu) do
        if i == selIndex then
            gfx.color(0xFFFFFFFF)
        else
            gfx.color(0xFFFFFF80)
        end
        gfx.fillText(font, item.x, item.y, item.label, 1)
    end
end

return scene
