-- arcualua port of arcaqjs graphics performance test

window.color(0x224466ff)
local numObj = 200
local objCounts = { 100, 200, 400, 500, 1000, 2000, 4000, 8000, 16000, 32000 }
local numComponents = 12

local counter, frames, fps, now = 0, 0, "", 0
local objs = {}
local sprites = {}
local LINE_SIZE = 24
local prevAxisY = 0

-- random int
local function randi(lo, hi)
    if not hi then
        return math.floor(math.random() * lo)
    end
    return lo + math.floor(math.random() * (hi - lo))
end

-- load image and quads
local sprites = resource.getTileGrid(resource.getImage('flags.png', 1, 0.5, 0.5), 6,5,2)

-- Sprite class
local Sprite = {}
Sprite.__index = Sprite

function Sprite:new(seed, winW, winH, szMin, szMax)
    local self = setmetatable({}, Sprite)
    local type_ = seed % 3

    if type_ == 0 then -- circle (quad index 29)
        self.image = sprites + 29
        self.color = randi(63,0xFFffFFff)
        self.rot = 0
        self.velRot = 0
    elseif type_ == 1 then -- rect (quad index 28)
        self.image = sprites + 28
        self.color = randi(63,0xFFffFFff)
        self.rot = 0
        self.velRot = 0
    else -- image
        self.image = sprites + (math.floor(seed/3) % 28)
        self.color = 0xFFffFFff
        self.rot = math.random() * math.pi * 2
        self.velRot = math.random() * math.pi - math.pi*0.5
    end

    self.x = randi(winW)
    self.y = randi(winH)
    self.velX = randi(-2*szMin, 2*szMin)
    self.velY = randi(-2*szMin, 2*szMin)
    self.id = seed

    return self
end

function Sprite:draw(gfx)
    gfx.color(self.color)
    gfx.drawImage(self.image, self.x, self.y, self.rot)
end

-- adjust object count
local function adjustNumObj(count)
    numObj = count
end

local prevAxisY = 0
function input(evt,device,id,value,value2)
	if evt == 'axis' and id == 1 then
		if value == -1.0 then
			if numObj > objCounts[1] then
				for i=2,#objCounts do
					if objCounts[i] == numObj then
						adjustNumObj(objCounts[i-1])
						break
					end
				end
			end
		elseif value == 1.0 then
			if numObj < objCounts[#objCounts] then
				for i=1,#objCounts-1 do
					if objCounts[i] == numObj then
						adjustNumObj(objCounts[i+1])
						break
					end
				end
			end
		end
		prevAxisY = value
	end
end

-- update loop
function update(dt)
    now = now + dt
    local winW, winH = window.width(), window.height()

    for i=1,numObj do
        local obj = objs[i]
        obj.x = obj.x + obj.velX * dt
        obj.y = obj.y + obj.velY * dt
        obj.rot = obj.rot + obj.velRot * dt

        if (obj.id + frames) % 15 == 0 then
            local r = 48*1.41
            if obj.x > winW + r then
                obj.x = -r
            elseif obj.x < -r then
                obj.x = winW + r
            end
            if obj.y > winH + r then
                obj.y = -r
            elseif obj.y < -r then
                obj.y = winH + r
            end
        end
    end

    counter = counter + 1
    frames = frames + 1
    if math.floor(now) ~= math.floor(now - dt) then
        fps = frames .. "fps"
        frames = 0
    end
    return true
end

-- draw loop
function draw(gfx)
    -- scene
    for i=1,numObj do
        objs[i]:draw(gfx)
    end

    -- overlay background
    gfx.color(0x7f7f7f7f)
    gfx.fillRect(0, 97, 115, #objCounts*LINE_SIZE)

    -- obj counts list
    for i,count in ipairs(objCounts) do
        if count == numObj then
            gfx.color(0xFFFFFFFF)
        else
            gfx.color(0xFFFF7F7F)
        end
        gfx.fillText(0, 0, 100 + (i-1)*LINE_SIZE, tostring(count))
    end

    -- bottom bar
    local winW, winH = window.width(), window.height()
    gfx.color(0x7f7f7f7f)
    gfx.fillRect(0, winH - LINE_SIZE - 2, winW, LINE_SIZE + 2)

    gfx.color(0xFFFFFFFF)
    gfx.fillText(0, 0, winH - 20, "arcalua graphics performance test")

    gfx.color(0xFF5555FF)
    gfx.fillText(0, winW - 60, winH - 20, fps)
end

-- init load()
function load()
    math.randomseed(os.time())
    local winW, winH = window.width(), window.height()
    for i=1,objCounts[#objCounts] do
        table.insert(objs, Sprite:new(i, winW, winH, 16, 64))
    end
    adjustNumObj(numObj)
end
