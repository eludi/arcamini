import math, random, window, resource, audio

winX, winY, sc = window.width(), window.height(), window.height()//240
grid = 10*sc
snake, food, dir, score, state, delay = [], (0,0), (1,0), 0, "play", 0
beep = resource.createAudio([0.3*math.sin(2*math.pi*660*n/44100) for n in range(2000)])
moop = resource.createAudio([math.sin(2*math.pi*220*n/44100) for n in range(10000)])

def load():
    global snake, dir, score, state, delay
    snake=[((winX//2//grid)*grid, (winY//2//grid)*grid)]
    dir=(1,0); score=0; state="play"; delay=0; newFood()

def newFood():
    global food
    food=(random.randint(0, winX//grid-1)*grid, random.randint(0, winY//grid-1)*grid)

def input(evt,dev,id,val,v2):
    global dir, state
    if state=="over" and evt=="button" and val>0: load()
    if evt=="axis" and abs(val)>0.5:
        dir = (int(val),0) if id%2 == 0 else (0,int(val))

def update(dt):
    global state, score, delay
    delay+=dt
    if delay<0.1 or state!="play": return True
    delay=0
    x,y=snake[0][0]+dir[0]*grid, snake[0][1]+dir[1]*grid
    if (x<0 or x+grid>winX or y<0 or y+grid>winY or (x,y) in snake):
        state="over"; audio.replay(moop); return True
    snake.insert(0,(x,y))
    if (x,y)==food: score+=1; audio.replay(beep); newFood()
    else: snake.pop()
    return True

def draw(gfx):
    gfx.color(0x202020ff); gfx.fillRect(0, 0, grid*(winX//grid), grid*(winY//grid))
    gfx.color(0x00ff00ff)
    for s in snake: gfx.fillRect(s[0],s[1],grid,grid)
    gfx.color(0xff0000ff); gfx.fillRect(food[0],food[1],grid,grid)
    gfx.color(0xffffffff); gfx.fillText(0,10*sc,20*sc,f"Score:{score}")
    if state=="over": gfx.fillText(0,winX//2,winY//2,"GAME OVER",1)
