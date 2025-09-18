import window

now = 0.0

def enter(arg):
    print("enter", arg)

def update(deltaT):
    global now
    now += deltaT

    if now > 3.0:
        window.switchScene("switchSceneDest.py", 'current_time', now)
    return True

def draw(gfx):
    gfx.color(0xFF5555FF)
    gfx.fillText(0, window.width()/2, window.height()/2, "original main file", 1) 

