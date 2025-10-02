from arcamini import window
from scene import switchScene, currentScene
from menu import Menu

# Current input state, updated by input events
inputs = [
    {'axes': [0, 0], 'buttons': [0, 0, 0, 0, 0, 0, 0, 0]}
]

def startup(args=None):
    window.color(0x000000FF)  # Set background color
    switchScene(Menu(), args or {})

def input(evt,device,id,value,value2):
    global inputs
    while device >= len(inputs):
        inputs.append({'axes': [0, 0], 'buttons': [0, 0, 0, 0, 0, 0, 0, 0]})
    if evt == 'axis':
        inputs[device]['axes'][id] = value
    elif evt == 'button':
        inputs[device]['buttons'][id] = value
    currScene = currentScene()
    if currScene and hasattr(currScene, 'input'):
        currScene.input(evt,device,id,value,value2)

def update(deltaT):
    currScene = currentScene()
    if currScene and hasattr(currScene, 'update'):
        return currScene.update(deltaT, inputs)
    return False

def draw(gfx):
    currScene = currentScene()
    if currScene and hasattr(currScene, 'draw'):
        currScene.draw(gfx)
