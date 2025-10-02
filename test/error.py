import random
from arcamini import window

def erroneous():
    raise RuntimeError("This is an error for testing purposes.")

def enter(args):
    window.color(0x0055aaff)
    print("enter called, args:", args)
    if random.random() < 0.5:
        erroneous()

def update(deltaT):
    print("update", deltaT)
    if random.random() < 0.5:
        if random.random() < 0.5:
            erroneous()
        else:
            window.color(None)
    return True

def draw(gfx):
    pass