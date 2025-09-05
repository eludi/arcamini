# game.py - arcapy/PocketPy port

from beeper import beeper
from scene import switchScene
from menu import Menu
import resource, window
import random

vpSz = 0
ox = 0
oy = 0
sc = 1
score = 0
highScore = 0
level = 0
delay = 0
objs = []
players = []
numPlayers = 1
bgColor = 0xFF
title = 0
velMax = 12.0

circleCache = {}
def fillCircle(gfx, x, y, radius):
    radius = int(radius + 0.5)
    img = circleCache.get(radius)
    if not img:
        svg = f'<svg width="{radius*2}" height="{radius*2}"><circle cx="{radius}" cy="{radius}" r="{radius}" fill="white" /></svg>'
        img = resource.createSVGImage(svg, 1, 0.5, 0.5)
        circleCache[radius] = img
    gfx.drawImage(img, x, y, radius)

def randFreq(base, low, high):
    if isinstance(low, list):
        return base * (2 ** (low[random.randint(0, len(low)-1)] / 12.0))
    return base * (2 ** ((random.randint(low, high)) / 12.0))

def drawFancyNumber(gfx, x, y, sc, number):
    ciphers =\
        "*** **  *** *** * * *** *   *** *** *** " +\
        "* *  *    *   * * * *   *     * * * * * " +\
        "* *  *  *** *** *** *** ***   * *** *** " +\
        "* *  *  *     *   *   * * *   * * *   * " +\
        "***  *  *** ***   * *** ***   * ***   * "
    stride = len(ciphers) // 5
    buf = str(number)
    for i in range(len(buf)):
        digit = int(buf[i]) if buf[i] in '0123456789' else None
        if digit is not None:
            for posY in range(5):
                for posX in range(4):
                    index = 4 * digit + posX + stride * posY
                    if ciphers[index] != ' ':
                        gfx.fillRect(x + posX * sc, y + posY * sc, sc - 1, sc - 1)
        x += 4 * sc

def randColor():
    r = random.randint(85, 255)
    g = random.randint(85, 255)
    b = 512 - r - g
    return (r << 24) | (g << 16) | (b << 8) | 255


# Highscore save/load
highscorePath = "highscore.dat"
def saveHighscore(score):
    resource.setStorageItem(highscorePath, str(score))
def loadHighscore():
    contents = resource.getStorageItem(highscorePath)
    return int(contents) if contents else 0

TYPE_PLAYER = 1
TYPE_BALL = 2

class GameObject:
    def __init__(self, x=0, y=0, radius=0, vx=0, vy=0, color=0xFFFFFFFF, type=0):
        self.x = x
        self.y = y
        self.radius = radius
        self.vx = vx
        self.vy = vy
        self.prevX = 0
        self.prevY = 0
        self.color = color
        self.type = type
        self.fixed = False

    def draw(self, gfx):
        if self.type == 0:
            return
        gfx.color(self.color)
        fillCircle(gfx, ox + self.x, oy + self.y, self.radius)

    def move(self, x, y):
        if self.type == 0:
            return
        self.prevX = self.x
        self.prevY = self.y
        self.x = x
        self.y = y

    def pan(self):
        return 2.0 * self.x / vpSz - 1.0 if vpSz else 0.0

    def update(self, deltaT):
        if self.type != TYPE_BALL:
            return True
        self.move(self.x + self.vx * deltaT, self.y + self.vy * deltaT)
        if ((self.x < self.radius and self.vx < 0) or (self.x >= vpSz - self.radius and self.vx > 0)):
            self.vx = -self.vx
            if self.y > self.radius:
                beeper.beep(randFreq(220, -2, 2), 0.015, 0.3, 0.5, self.pan())
        if self.y <= -self.radius and self.vy < 0:
            return False
        return self.y < vpSz + self.radius

    def intersects(self, other):
        if self.type == 0 or other.type == 0:
            return False
        dx = other.x - self.x
        dy = other.y - self.y
        return (dx * dx + dy * dy) < (self.radius + other.radius) ** 2

    def resolveCollision(self, other):
        dx = other.x - self.x
        dy = other.y - self.y
        dist = (dx ** 2 + dy ** 2) ** 0.5
        if dist == 0 or (getattr(self, 'fixed', False) and getattr(other, 'fixed', False)):
            return
        nx = dx / dist
        ny = dy / dist
        tx = -ny
        ty = nx
        v1n = self.vx * nx + self.vy * ny
        v1t = self.vx * tx + self.vy * ty
        v2n = other.vx * nx + other.vy * ny
        v2t = other.vx * tx + other.vy * ty
        m1 = 0 if getattr(other, 'fixed', False) else self.radius ** 2
        m2 = 0 if getattr(self, 'fixed', False) else other.radius ** 2
        if m1 + m2 == 0:
            return
        v1nAfter = (v1n * (m1 - m2) + 2 * m2 * v2n) / (m1 + m2)
        v2nAfter = (v2n * (m2 - m1) + 2 * m1 * v1n) / (m1 + m2)
        self.vx = v1nAfter * nx + v1t * tx
        self.vy = v1nAfter * ny + v1t * ty
        other.vx = v2nAfter * nx + v2t * tx
        other.vy = v2nAfter * ny + v2t * ty
        overlap = self.radius + other.radius - dist
        if overlap > 0:
            correctionFactor = overlap / (m1 + m2)
            self.x -= nx * correctionFactor * m2
            self.y -= ny * correctionFactor * m2
            other.x += nx * correctionFactor * m1
            other.y += ny * correctionFactor * m1

class Game:
    @staticmethod
    def nextLevel():
        global level, bgColor, objs, sc, vpSz
        level += 1
        bgColor = randColor()
        for i in range(level):
            objs.append(GameObject(
                random.random() * vpSz,
                random.random() * -8 * sc,
                sc * 0.5,
                random.random() * 4 * sc - 2 * sc,
                random.random() * 2 * sc + 2 * sc,
                0xff,
                TYPE_BALL
            ))

    @staticmethod
    def over(restart):
        global score, highScore, level, players, objs, numPlayers
        if score > highScore:
            highScore = score
            saveHighscore(highScore)
        if not restart:
            return switchScene(Menu())
        score = 0
        level = 0
        players = []
        for i in range(numPlayers):
            players.append(GameObject(vpSz/2, vpSz/2, sc, 0, 0, 0xFFffFFff, TYPE_PLAYER))
        objs = []
        objs.extend(players)
        Game.nextLevel()

    @staticmethod
    def enter(args):
        global numPlayers, vpSz, ox, oy, sc, title, highScore
        if args and 'players' in args:
            numPlayers = int(args['players']) if args['players'] else 1
        winSzX = window.width()
        winSzY = window.height()
        vpSz = min(winSzX, winSzY)
        ox = (winSzX - vpSz) / 2
        oy = (winSzY - vpSz) / 2
        sc = vpSz / 30
        title = resource.getImage("BALLATTAX.svg", vpSz/512, 0.5, 1)
        highScore = loadHighscore()
        Game.over(True)

    @staticmethod
    def update(dt, input):
        global delay, players, objs, score, level
        beeper.update(dt)
        if delay > 0:
            delay -= dt
            if delay <= 0:
                delay = 0
                Game.over(True)
            return True
        for i, player in enumerate(players):
            if i < len(input):
                if input[i]['buttons'][6] == 1 and input[i]['buttons'][7] == 1:
                    Game.over(False)
                player.vx = input[i]['axes'][0] * velMax * sc
                player.vy = input[i]['axes'][1] * velMax * sc
        for player in players:
            player.move(player.x + player.vx * dt, player.y + player.vy * dt)
            if player.x < player.radius:
                player.x = player.radius
            elif player.x > vpSz - player.radius:
                player.x = vpSz - player.radius
            if player.y < player.radius:
                player.y = player.radius
            elif player.y > vpSz - player.radius:
                player.y = vpSz - player.radius
        i = len(objs) - 1
        while i >= 0:
            obj = objs[i]
            if obj.type == TYPE_BALL and not obj.update(dt):
                if obj.vy < 0:
                    score += 1
                    freq = 880
                    beeper.play({'freq': freq, 'duration': 0.05, 'vol': 0.1, 'pan': obj.pan()}, {'freq': 2 * freq, 'deltaT': 0.05})
                    objs.pop(i)
                    if len(objs) == len(players):
                        Game.nextLevel()
                else:
                    beeper.beep(55, 0.5, 0.3, 0.5, 0.0)
                    delay = 0.5
                break
            for j in range(i):
                if objs[i].intersects(objs[j]):
                    objs[i].resolveCollision(objs[j])
                    if objs[i].y > 0:
                        if objs[i].type == TYPE_BALL and objs[j].type == TYPE_BALL:
                            beeper.beep(randFreq(880, -3, 3), 0.015, 0.2, 0.5, objs[i].pan())
                        elif objs[i].type == TYPE_BALL or objs[j].type == TYPE_BALL:
                            beeper.beep(randFreq(110, -1, 3), 0.025, 0.3, 0.5, objs[i].pan())
            i -= 1
        return True

    @staticmethod
    def draw(gfx):
        gfx.color(bgColor)
        gfx.fillRect(ox, oy, vpSz, vpSz)
        gfx.color(0xFFffFF55)
        gfx.drawImage(title, ox + vpSz / 2, oy + vpSz)
        gfx.color(0xFFffFF55)
        drawFancyNumber(gfx, ox + vpSz * 0.333 - 2.5 * sc, oy + 2 * sc, sc, highScore)
        gfx.color(0xFFffFFaa)
        drawFancyNumber(gfx, ox + vpSz * 0.667 - 2.5 * sc, oy + 2 * sc, sc, score)
        for obj in objs:
            obj.draw(gfx)

