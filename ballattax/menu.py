# menu.py - arcapy/PocketPy port

from beeper import beeper
import resource, window
from scene import switchScene

bgColor = 0xFF4080FF
winSzX = window.width()
winSzY = window.height()
sc = min(winSzX / 640, winSzY / 480)
title = resource.getImage('BALLATTAX.svg', sc, 0.5, 0.5)
font = resource.getFont('SpaceGrotesk-Bold.ttf', int(32 * sc))
selIndex = -1
menu = [ {'label': 'single player'}, {'label': 'multiplayer'}, {'label': 'exit'} ]
prevInputY = 0
running = True

def triggerMenuEvent(index):
    global running
    if index < 0:
        return
    elif index == 2:
        running = False
        return
    from game import Game
    switchScene(Game, {'players': index + 1})

class Menu:
    def enter(self, args):
        for i in range(len(menu)):
            menu[i]['x'] = winSzX / 2
            menu[i]['y'] = winSzY / 2 + 40 * sc * (i + 1)

    def update(self, dt, inputs):
        global selIndex, prevInputY, running
        #print(f"Menu.update({dt}, {inputs})")
        if inputs[0]['buttons'][0] != 0:
            triggerMenuEvent(selIndex)
        if prevInputY == 0 and inputs[0]['axes'][1] != 0:
            beeper.beep(60 if inputs[0]['axes'][1] < 0 else 120, 0.01, 0.2)
            selIndex += 1 if inputs[0]['axes'][1] > 0 else -1
            if selIndex >= len(menu):
                selIndex = 0
            elif selIndex < 0:
                selIndex = len(menu) - 1
        prevInputY = inputs[0]['axes'][1]
        return running

    def draw(self, gfx):
        gfx.color(bgColor)
        gfx.fillRect(0, 0, winSzX, winSzY)
        gfx.color(0xFFFFFFFF)
        gfx.drawImage(title, winSzX / 2, winSzY * 0.3)
        for i in range(len(menu)):
            gfx.color(0xFFFFFFFF if i == selIndex else 0xFFFFFF80)
            #print(f"gfxFillText({font}, {menu[i]['x']}, {menu[i]['y']}, {menu[i]['label']}, 1)")
            gfx.fillText(font, menu[i]['x'], menu[i]['y'], menu[i]['label'], 1)

