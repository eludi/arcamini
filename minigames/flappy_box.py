import random, math
from arcamini import window, resource, audio

winSzX, winSzY, sc = window.width(), window.height(), window.height()/480
player, pipes, score, state = {"y":0,"vy":0}, [], 0, "play"
notes = [resource.createAudio([math.sin(2*math.pi*f*n/44100)
    for n in range(8000 if f==220 else 2000)]) for f in (220,440,554,659,880)]

def enter(args):
    global player, pipes, score, state
    player["y"]=winSzY//2; player["vy"]=0; pipes.clear(); score=0; state="play"
    window.color(0x202020ff)

def input(evt,dev,id,val,v2=None):
    if state=="over" and evt=="button" and val>0: enter(None)
    elif state=="play" and evt=="button" and val>0: player["vy"]=-250*sc

def update(dt):
    global score, state
    if state!="play": return True
    player["vy"] += 600*dt*sc; player["y"] += player["vy"]*dt
    if not pipes or pipes[-1][0] < winSzX-200*sc:
        gap = random.randint(int(100*sc), winSzY-int(100*sc)); pipes.append([winSzX,gap])
    for p in list(pipes):
        p[0] -= 150*dt*sc
        if p[0]+40*sc < 0: pipes.remove(p); audio.replay(notes[1+score%(len(notes)-1)], 0.5); score+=1
        if (20*sc < p[0] < 80*sc) and not(p[1]-60*sc < player["y"] < p[1]+30*sc):
            state="over"; audio.replay(notes[0])
    if player["y"] < 0 or player["y"] > winSzY-30*sc: state="over"; audio.replay(notes[0])
    return True

def draw(g):
    g.color(0x00ff00ff); g.fillRect(50*sc,player["y"],30*sc,30*sc)
    g.color(0xff0000ff)
    for p in pipes:
        g.fillRect(p[0],0,40*sc,p[1]-60*sc); g.fillRect(p[0],p[1]+60*sc,40*sc,winSzY-p[1]-60*sc)
    g.color(0xffffffff); g.fillText(0,10*sc,20*sc,f"Score:{score}")
    if state=="over": g.fillText(0,winSzX//2,winSzY//2,"GAME OVER",1)
