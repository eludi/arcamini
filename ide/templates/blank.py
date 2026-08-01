from arcamini import window, resource, audio

def enter(args):
    window.color(0x202020ff)

def update(dt):
    return True

def draw(gfx):
    gfx.color(0x40c0ffff)
    gfx.fillRect(window.width() / 2 - 25, window.height() / 2 - 25, 50, 50)
