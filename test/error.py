import random, window

def erroneous():
    raise RuntimeError("This is an error for test purposes.")

def load():
    print("load")
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
