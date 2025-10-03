import random, math
from arcamini import resource, window

window.color(0x224466ff)
numObj = 200
objCounts = [100, 200, 400, 500, 1000, 2000, 4000, 8000, 16000, 32000]

def randi(lo, hi=None):
    if hi is None:
        return int(random.random() * lo)
    return lo + int(random.random() * (hi - lo))

counter = 0
frames = 0
fps = ''
now = 0

sprites = resource.getTileGrid(resource.getImage('flags.png', 1, 0.5, 0.5), 6, 5, 2)

LINE_SIZE = 24

class Sprite:
    def __init__(self, seed, winSzX, winSzY, szMin, szMax):
        type_ = seed % 3
        if type_ == 0:  # circle
            self.image = sprites + 29
            self.color = randi(63, 0xFFFFFFFF)
            self.rot = 0
            self.velRot = 0
        elif type_ == 1:  # rect
            self.image = sprites + 28
            self.color = randi(63, 0xFFFFFFFF)
            self.rot = 0
            self.velRot = 0
        elif type_ == 2:  # img
            self.image = sprites + ((seed // 3) % 28)
            self.color = 0xFFFFFFFF
            self.rot = random.random() * math.pi * 2
            self.velRot = random.random() * math.pi - math.pi * 0.5

        self.x = randi(winSzX)
        self.y = randi(winSzY)
        self.velX = randi(-2 * szMin, 2 * szMin)
        self.velY = randi(-2 * szMin, 2 * szMin)
        self.id = seed

    def draw(self, gfx):
        gfx.color(self.color)
        gfx.drawImage(self.image, self.x, self.y, self.rot)

objs = []

def adjustNumObj(count):
    global numObj
    numObj = count

prevAxisY = 0
def input(evt,device,id,value,value2):
    global prevAxisY, numObj
    if evt == 'axis' and id == 1:
        if value == -1.0:
            if numObj > objCounts[0]:
                for i in range(1, len(objCounts)):
                    if objCounts[i] == numObj:
                        adjustNumObj(objCounts[i-1])
                        break
        elif value == 1.0:
            if numObj < objCounts[-1]:
                for i in range(len(objCounts)-1):
                    if objCounts[i] == numObj:
                        adjustNumObj(objCounts[i+1])
                        break
        prevAxisY = value

def update(deltaT):
    global now, counter, frames, fps
    now += deltaT
    for i in range(numObj):
        obj = objs[i]
        obj.x += obj.velX * deltaT
        obj.y += obj.velY * deltaT
        obj.rot += obj.velRot * deltaT

        if (obj.id + frames) % 15 == 0:
            r = 48 * 1.41
            if obj.x > window.width() + r:
                obj.x = -r
            elif obj.x < -r:
                obj.x = window.width() + r
            if obj.y > window.height() + r:
                obj.y = -r
            elif obj.y < -r:
                obj.y = window.height() + r

    counter += 1
    frames += 1
    if int(now) != int(now - deltaT):
        fps = f"{frames}fps"
        frames = 0
    return True

def draw(gfx):
    # scene:
    for i in range(numObj):
        objs[i].draw(gfx)

    # overlay:
    gfx.color(0x7f)
    gfx.fillRect(0, 97, 115, len(objCounts) * LINE_SIZE)
    for i, count in enumerate(objCounts):
        gfx.color(0xFFFFFFFF if count == numObj else 0xFFFFFF7F)
        gfx.fillText(0, 0, 100 + i * LINE_SIZE, count)

    gfx.color(0x7f)
    gfx.fillRect(0, window.height() - LINE_SIZE - 2, window.width(), LINE_SIZE + 2)
    gfx.color(0xFFFFFFFF)
    gfx.fillText(0, 0, window.height() - 20, "arcapy graphics performance test")
    gfx.color(0xFF5555FF)
    gfx.fillText(0, window.width() - 60, window.height() - 20, fps)

for i in range(objCounts[-1]):
    objs.append(Sprite(i, window.width(), window.height(), 16, 64))
adjustNumObj(numObj)
