local now = 0.0
print('yay, switchScene worked!')

function enter(args)
    print('enter called with args:', args)
end

function update(deltaT)
    now = now + deltaT
    return true
end

function draw(gfx)
    local color = (math.fmod(now, 1.0) < 0.5) and 0xFFFFFFFF or 0x55FF55FF
    gfx.color(color)
    gfx.fillText(0, window.width()/2, window.height()/2, 'yay, switchScene worked!', 1)
end

function leave()
    print('leave called')
end
