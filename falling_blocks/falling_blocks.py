import random, math, window, resource, audio

blocks, score, highscore, state = [], 0, 0, "play"
font, winSzX, winSzY, sc = 0, window.width(), window.height(), window.width() / 640
player = {"x": winSzX // 2, "y": winSzY - 40 * sc, "w": 40 * sc, "h": 20 * sc, "vx": 0}

def beep(freq, dur, vol=1.0):
    return resource.createAudio([math.sin(2 * math.pi * freq * n / 44100) * vol
        for n in range(int(44100 * dur))])
hit, notes = beep(220, .2, .8), [beep(f, .05, .2) for f in (440, 494, 554, 659, 740, 880)]

def load():
    window.color(0x202020ff)
    h = resource.getStorageItem("highscore")
    global highscore; highscore = int(h) if h else 0

def unload():
    resource.setStorageItem("highscore", str(score if score > highscore else highscore))

def input(evt, dev, id, val, v2):
    global state, blocks, score
    if state == "over" and evt == "button" and val > 0:
        blocks.clear(); score = 0; state = "play"
        player['x'] = int(winSzX // 2); player['vx'] = 0
    elif state == "play" and evt == "axis" and id == 0: player["vx"] = val * 200 * sc

def update(dt):
    global score, state, highscore, hit, notes
    if state != "play": return True
    player["x"] = max(0, min(winSzX - player["w"], player["x"] + player["vx"] * dt))
    if random.random() < 0.01 + score * 0.001:
        blocks.append([random.randint(0, winSzX - int(20 * sc)), -int(20 * sc)])
    for b in list(blocks):
        b[1] += 150 * sc * dt
        if b[1] > winSzY:
            blocks.remove(b); score += 1
            audio.replay(notes[int(b[0] // (winSzX // len(notes)))])
        elif player["x"] < b[0] + 20 * sc and b[0] < player["x"] + player["w"] and\
             player["y"] < b[1] + 20 * sc and b[1] < player["y"] + player["h"]:
            if score > highscore: highscore = score
            state = "over"; audio.replay(hit)
    return True

def draw(gfx):
    gfx.color(0x00ff00ff); gfx.fillRect(player["x"], player["y"], player["w"], player["h"])
    gfx.color(0xff0000ff)
    for b in blocks: gfx.fillRect(b[0], b[1], 20 * sc, 20 * sc)
    gfx.color(0xffffffaa); gfx.fillText(font, 10 * sc, 20 * sc, f"Score: {score}  High: {highscore}")
    if state == "over":
        gfx.fillText(font, winSzX / 2, winSzY / 2, "GAME OVER - press any button", 1)