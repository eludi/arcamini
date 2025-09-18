let blocks = [], score = 0, highscore = 0, state = "play";
const winSzX = window.width(), winSzY = window.height(), sc = winSzX / 640;
let player = { x: Math.floor(winSzX / 2), y: winSzY - 40 * sc, w: 40 * sc, h: 20 * sc, vx: 0 };

function beep(freq, dur, vol = 1.0) {
    let data = [];
    for (let n = 0; n < Math.floor(44100 * dur); ++n)
        data.push(Math.sin(2 * Math.PI * freq * n / 44100) * vol);
    return resource.createAudio(data);
}
const hit = beep(220, 0.2, 0.8);
const notes = [440, 494, 554, 659, 740, 880].map(f => beep(f, 0.05, 0.2));

function startup() {
    window.color(0x202020ff);
    let h = resource.getStorageItem("highscore");
    highscore = h ? parseInt(h) : 0;
}

function shutdown() {
    resource.setStorageItem("highscore", String(score > highscore ? score : highscore));
}

function input(evt, dev, id, val, v2) {
    if (state === "over" && evt === "button" && val > 0) {
        blocks = []; score = 0; state = "play";
        player.x = Math.floor(winSzX / 2); player.vx = 0;
    } else if (state === "play" && evt === "axis" && id === 0)
        player.vx = val * 200 * sc;
}

function update(dt) {
    if (state !== "play") return true;
    player.x = Math.max(0, Math.min(winSzX - player.w, player.x + player.vx * dt));
    if (Math.random() < 0.01 + score * 0.001)
        blocks.push([Math.floor(Math.random() * (winSzX - 20 * sc)), -20 * sc]);
    for (let i = blocks.length - 1; i >= 0; --i) {
        let b = blocks[i];
        b[1] += 150 * sc * dt;
        if (b[1] > winSzY) {
            blocks.splice(i, 1); score += 1;
            audio.replay(notes[Math.floor(b[0] / (winSzX / notes.length))]);
        } else if (player.x < b[0] + 20 * sc && b[0] < player.x + player.w &&
                   player.y < b[1] + 20 * sc && b[1] < player.y + player.h) {
            if (score > highscore) highscore = score;
            state = "over"; audio.replay(hit);
        }
    }
    return true;
}

function draw(gfx) {
    gfx.color(0x00ff00ff); gfx.fillRect(player.x, player.y, player.w, player.h);
    gfx.color(0xff0000ff);
    for (let b of blocks) gfx.fillRect(b[0], b[1], 20 * sc, 20 * sc);
    gfx.color(0xffffffaa);
    gfx.fillText(0, 10 * sc, 20 * sc, `Score: ${score}  High: ${highscore}`);
    if (state === "over")
        gfx.fillText(0, winSzX / 2, winSzY / 2, "GAME OVER - press any button", 1);
}
