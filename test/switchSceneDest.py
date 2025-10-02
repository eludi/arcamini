import math, sys
from arcamini import window

now = 0.0
print("yay, switchScene worked!")

def enter(args):
    print("enter called with args:", args)

def update(deltaT):
    global now
    now += deltaT
    return True

def draw(gfx):
    gfx.color(0xFFFFFFFF if math.fmod(now,1.0) < 0.5 else 0x55FF55FF)
    gfx.fillText(0, window.width()/2, window.height()/2, "yay, switchScene worked!", 1)

def leave():
    print("leave called")
