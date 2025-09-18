now = 0.0

function enter(arg)
    print('enter', arg)
end

function update(deltaT)
    now = now + deltaT
    if now > 3.0 then
        window.switchScene('switchSceneDest.lua', 'current_time', now)
    end
    return true
end

function draw(gfx)
    gfx.color(0xFF5555FF)
    gfx.fillText(0, window.width()/2, window.height()/2, 'original main file', 1)
end
