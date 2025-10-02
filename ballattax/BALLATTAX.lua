-- Arcalua version of ballattax

function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..tostring(k)..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ', '
      end
      return s .. '}'
   else
      return tostring(o)
   end
end

local currentScene

-- current input state, updated by input events
local inputs = {
    {axes = {0, 0}, buttons = {0, 0, 0, 0, 0, 0, 0, 0}}
}

-- Scene management, dispatch events to game.lua and menu.lua
function switchScene(newScene, args)
    if currentScene and type(currentScene.exit) == "function" then
        currentScene.exit()
    end
    currentScene = newScene
    if currentScene and type(currentScene.enter) == "function" then
        currentScene.enter(args)
    end
end

-- Lifecycle callbacks
function enter(args)
    window.color(0x000000FF) -- Set background color
    switchScene(require(args.scene or "menu"), args)
end

function input(evt,device,id,value,value2)
    if currentScene and currentScene.input then
        currentScene.input(evt,device,id,value,value2)
    end
    -- update inputs state, evt describes a state change:
    if device+1 > #inputs then
        inputs[device+1] = {axes = {0, 0}, buttons = {0, 0, 0, 0, 0, 0, 0, 0}}
    end
    if evt == "axis" then
        inputs[device+1].axes[id+1] = value
    elseif evt == "button" then
        inputs[device+1].buttons[id+1] = value
    end
end

function update(deltaT)
    if currentScene and currentScene.update then
        return currentScene.update(deltaT, inputs)
    end
    return false
end

function draw(gfx)
    if currentScene and currentScene.draw then
        currentScene.draw(gfx)
    end
end
