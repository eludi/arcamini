-- blabawhiba.lua - Arcalua port

local vpSz, ox, oy, sc = 0, 0, 0, 1
local score, highScore, level, delay = 0, 0, 0, 0
local objs = {}
local players = {}
local numPlayers = 1
local bgColor = 0xFF
local info = ''

local circleCache = {}
function fillCircle(gfx, x, y, radius)
    radius = math.floor(radius+0.5)
    local img = circleCache[radius]
    if not img then -- create circle as SVG image and store it in circleCache
        local svg = string.format('<svg width="%d" height="%d"><circle cx="%d" cy="%d" r="%d" fill="white" /></svg>', radius*2, radius*2, radius, radius, radius)
        img = resource.createSVGImage(svg, 1, 0.5, 0.5)
        circleCache[radius] = img
    end 
    gfx.drawImage(img, x, y, radius)
end

-- -------------------------

local beeper = require('beeper')

function randFreq(base, low, high)
    if type(low) == 'table' then
        return base * 2 ^ (low[math.random(#low)] / 12.0)
    end
    return base * 2 ^ (math.random(low, high) / 12.0)
end

function drawFancyNumber(gfx, x, y, sc, number)
    local ciphers =
        "*** **  *** *** * * *** *   *** *** *** " ..
        "* *  *    *   * * * *   *     * * * * * " ..
        "* *  *  *** *** *** *** ***   * *** *** " ..
        "* *  *  *     *   *   * * *   * * *   * " ..
        "***  *  *** ***   * *** ***   * ***   * "

    local stride = #ciphers / 5
    local buf = tostring(number)

    for i = 1, #buf do
        local digit = tonumber(buf:sub(i, i))
        if digit then
            for posY = 0, 4 do
                for posX = 0, 3 do
                    local index = 4 * digit + posX + stride * posY + 1 -- Lua is 1-based
                    if ciphers:sub(index, index) ~= ' ' then
                        gfx.fillRect(x + posX * sc, y + posY * sc, sc - 1, sc - 1)
                    end
                end
            end
        end
        x = x + 4 * sc
    end
end

function randColor()
    local r = math.floor(math.random()*170) + 85
    local g = math.floor(math.random()*170) + 85
    local b = 512 - r - g
    return (r << 24) | (g << 16) | (b << 8) | 255
end

local highscorePath = "highscore.dat"
function saveHighscore(score)
    resource.setStorageItem(highscorePath, tostring(score))
end

function loadHighscore()
    local contents = resource.getStorageItem(highscorePath)
    return contents and tonumber(contents) or 0
end

-- -------------------------
-- Game object structure and functions

TYPE_PLAYER = 1
TYPE_BALL = 2

local GameObject = {}
GameObject.__index = GameObject

function GameObject:new(x, y, radius, vx, vy, color, type)
    local obj = setmetatable({}, self)
    obj.x = x or 0
    obj.y = y or 0
    obj.radius = radius or 0
    obj.vx = vx or 0
    obj.vy = vy or 0
    obj.prevX = 0
    obj.prevY = 0
    obj.color = color or 0xFFFFFFFF
    obj.type = type or 0
    return obj
end

function GameObject:draw(gfx)
    if self.type == 0 then return end
    gfx.color(self.color)
    fillCircle(gfx, ox + self.x, oy + self.y, self.radius)
end

function GameObject:move(x, y)
    if self.type == 0 then return end
    self.prevX, self.prevY = self.x, self.y
    self.x, self.y = x, y
end

function GameObject:pan()
    return 2.0 * self.x / vpSz - 1.0
end

function GameObject:update(deltaT)
    if self.type ~= TYPE_BALL then return true end
    self:move(self.x + self.vx * deltaT, self.y + self.vy * deltaT)
    if (self.x < self.radius and self.vx < 0) or (self.x >= vpSz - self.radius and self.vx > 0) then
        self.vx = -self.vx
        if self.y > self.radius then
            beeper:beep(randFreq(220, -2, 2), 0.015, 0.3, 0.5, self:pan())
        end
    end
    if self.y <= -self.radius and self.vy < 0 then
        return false
    end
    return self.y < vpSz + self.radius
end

function GameObject:intersects(other)
    if self.type == 0 or other.type == 0 then return false end
    local dx = other.x - self.x
    local dy = other.y - self.y
    return (dx * dx + dy * dy) < (self.radius + other.radius) ^ 2
end

function GameObject:resolveCollision(other)
    local dx = other.x - self.x
    local dy = other.y - self.y
    local dist = math.sqrt(dx * dx + dy * dy)
    if dist == 0 or (self.fixed and other.fixed) then return end

    local nx = dx / dist
    local ny = dy / dist
    local tx = -ny
    local ty = nx

    local v1n = self.vx * nx + self.vy * ny
    local v1t = self.vx * tx + self.vy * ty
    local v2n = other.vx * nx + other.vy * ny
    local v2t = other.vx * tx + other.vy * ty

    local m1 = other.fixed and 0 or (self.radius ^ 2)
    local m2 = self.fixed and 0 or (other.radius ^ 2)

    local v1nAfter = (v1n * (m1 - m2) + 2 * m2 * v2n) / (m1 + m2)
    local v2nAfter = (v2n * (m2 - m1) + 2 * m1 * v1n) / (m1 + m2)

    self.vx = v1nAfter * nx + v1t * tx
    self.vy = v1nAfter * ny + v1t * ty
    other.vx = v2nAfter * nx + v2t * tx
    other.vy = v2nAfter * ny + v2t * ty

    local overlap = self.radius + other.radius - dist
    if overlap > 0 then
        local correctionFactor = overlap / (m1 + m2)
        self.x = self.x - nx * correctionFactor * m2
        self.y = self.y - ny * correctionFactor * m2
        other.x = other.x + nx * correctionFactor * m1
        other.y = other.y + ny * correctionFactor * m1
    end
end


-- -------------------------
-- game
local Game = {}

function Game.nextLevel()
    level = level + 1
    bgColor = randColor()
    for i = 1, level do
        table.insert(objs, GameObject:new(
            math.random(0, vpSz), math.random(-8*sc, 0), sc*0.5, -- x, y, radius
            math.random(-2*sc, 2*sc), math.random(2*sc, 4*sc), -- vx, vy
            0xff, TYPE_BALL))
    end
end

function Game.over(restart)
    if score > highScore then
        highScore = score
        saveHighscore(highScore)
    end
    if not restart then
        return switchScene(require("menu"))
    end
    score = 0
    level = 0
    players = { }
    for i = 1, numPlayers do
        table.insert(players, GameObject:new(vpSz/2, vpSz/2, sc, 0, 0, 0xFFffFFff, TYPE_PLAYER))
    end

    objs = { }
    for i,v in ipairs(players) do
        table.insert(objs, v)
    end
    Game.nextLevel()
end

function Game.enter(args)
    if args and args.players then
        numPlayers = tonumber(args.players) or 1
    end
    local winSzX, winSzY = window.width(), window.height()
    vpSz = math.min(winSzX, winSzY)
    ox = (winSzX - vpSz) / 2
    oy = (winSzY - vpSz) / 2
    sc = vpSz / 30
    title = resource.getImage("BALLATTAX.svg", vpSz/512, 0.5,1)
    velMax = 12.0
    highScore = loadHighscore()
    Game.over(true)
end

function Game.update(dt, input)
    beeper:update(dt)
    if delay > 0 then
        delay = delay - dt
        if delay <= 0 then
            delay = 0
            Game.over(true)
        end
        return true
    end
    -- input handling
    for i, player in ipairs(players) do
        if i <= #input then
            if input[i].buttons[7] ==1 and input[i].buttons[8] == 1 then
                Game.over(false)
            end
            player.vx = input[i].axes[1] * velMax * sc
            player.vy = input[i].axes[2] * velMax * sc
        end
    end
    
    for i, player in ipairs(players) do -- update players
        player:move(player.x + player.vx * dt, player.y + player.vy * dt)
        if player.x < player.radius then player.x = player.radius
        elseif player.x > vpSz - player.radius then player.x = vpSz - player.radius end
        if player.y < player.radius then player.y = player.radius
        elseif player.y > vpSz - player.radius then player.y = vpSz - player.radius end
    end

    -- Update balls
    for i = #objs,1,-1 do
        local obj = objs[i]
        if obj.type == TYPE_BALL and not obj:update(dt) then
            if obj.vy < 0 then
                score = score + 1
                local freq = 880 -- randFreq(880,{0,4,7})
                beeper:play({freq=freq, duration=0.05, vol=0.1, pan=obj:pan()}, {freq=2*freq, deltaT=0.05})
                table.remove(objs, i)
                if #objs == #players then
                    Game.nextLevel()
                end
            else -- game over
                beeper:beep(55, 0.5, 0.3, 0.5, 0.0)
                delay = 0.5
            end
            break
        end
        for j = 1,i - 1 do
            if objs[i]:intersects(objs[j]) then
                objs[i]:resolveCollision(objs[j])
                if objs[i].y > 0 then
                    if objs[i].type == TYPE_BALL and objs[j].type == TYPE_BALL then
                        beeper:beep(randFreq(880, -3, 3), 0.015, 0.2, 0.5, objs[i]:pan())
                    elseif objs[i].type == TYPE_BALL or objs[j].type == TYPE_BALL then
                        beeper:beep(randFreq(110, -1, 3), 0.025, 0.3, 0.5, objs[i]:pan())
                    end
                end
            end
        end
    end
    return true
end

function Game.draw(gfx)
    gfx.color(bgColor)
    gfx.fillRect(ox, oy, vpSz, vpSz)
    gfx.color(0xFFffFF55)
    gfx.drawImage(title,ox+vpSz/2,oy+vpSz)
	
    gfx.color(0xFFffFF55)
    drawFancyNumber(gfx, ox+vpSz*0.333-2.5*sc, oy+2*sc, sc, highScore)
    gfx.color(0xFFffFFaa)
    drawFancyNumber(gfx, ox+vpSz*0.667-2.5*sc, oy+2*sc, sc, score)

    for _, obj in ipairs(objs) do
        obj:draw(gfx)
    end
end

return Game
