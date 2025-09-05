currScene = None

def switchScene(newScene, args=None):
    global currScene
    if currScene and hasattr(currScene, 'exit'):
        currScene.exit()
    currScene = newScene
    if currScene and hasattr(currScene, 'enter'):
        currScene.enter(args or {})

def currentScene():
    return currScene