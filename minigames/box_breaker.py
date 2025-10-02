import random, math
from arcamini import window, resource, audio

winX, winY, sc = window.width(), window.height(), window.height()/480
ball, paddle, bricks, score, state = {"x":0,"y":0,"vx":0,"vy":0}, {"w":80*sc,"h":10*sc,"vx":0}, [], 0, "play"
hit = resource.createAudio([math.sin(2*math.pi*440*n/44100) for n in range(int(44100*0.05))])

def startup():
    global ball, paddle, bricks, score, state
    w,h=winX//10,20*sc
    bricks=[]
    for y in range(5): for x in range(10): bricks.append([x * w, y * h])
    paddle["x"]=winX//2-40*sc; paddle["y"]=winY-30*sc
    ball["x"]=winX//2; ball["y"]=winY//2; ball["vx"]=120*sc; ball["vy"]=-120*sc
    score=0; state="play"; window.color(0x202020ff)

def input(evt,dev,id,val,v2):
    if state=="over" and evt=="button" and val>0: startup()
    elif evt=="axis" and id%2==0: paddle["vx"] = val * paddle["w"] * 4

def update(dt):
    global score,state
    if state!="play": return True
    ball["x"]+=ball["vx"]*dt; ball["y"]+=ball["vy"]*dt
    if ball["x"]<0 or ball["x"]>winX-10*sc: ball["vx"]*=-1
    if ball["y"]<0: ball["vy"]*=-1
    if ball["y"]>winY: state="over"
    paddle["x"] = max(0,min(winX-paddle["w"], paddle["x"]+paddle["vx"]*dt))
    if (paddle["x"]<ball["x"]<paddle["x"]+paddle["w"] and\
        paddle["y"]<ball["y"]+10*sc<paddle["y"]+paddle["h"]):
        ball["vx"] += (paddle["x"]+paddle["w"]/2 - ball["x"])/(paddle["w"]/2) * -120*sc
        ball["vy"]=-abs(ball["vy"]*1.05); audio.replay(hit)
    for b in list(bricks):
        if b[0]<ball["x"]<b[0]+winX//10 and b[1]<ball["y"]<b[1]+20*sc:
            bricks.remove(b); ball["vy"]*=-1; score+=1; audio.replay(hit)
    return True

def draw(g):
    g.color(0xffffffff); g.fillRect(ball["x"],ball["y"],10*sc,10*sc)
    g.color(0x00ff00ff); g.fillRect(paddle["x"],paddle["y"],paddle["w"],paddle["h"])
    g.color(0xff0000ff)
    for b in bricks: g.fillRect(b[0],b[1],winX//10-2,20*sc-2)
    g.color(0xffffffff); g.fillText(0,10,20,f"Score:{score}")
    if state=="over": g.fillText(0,winX//2,winY//2,"GAME OVER",1)
