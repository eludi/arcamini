// game.js - Arcaqjs/QuickJS port

const beeper = require('beeper.js');

let vpSz = 0, ox = 0, oy = 0, sc = 1;
let score = 0, highScore = 0, level = 0, delay = 0;
let objs = [];
let players = [];
let numPlayers = 1;
let bgColor = 0xFF;
let title = 0;
const velMax = 12.0;

const circleCache = {};
function fillCircle(gfx, x, y, radius) {
    radius = Math.floor(radius + 0.5);
    let img = circleCache[radius];
    if (!img) {
        const svg = `<svg width="${radius*2}" height="${radius*2}"><circle cx="${radius}" cy="${radius}" r="${radius}" fill="white" /></svg>`;
        img = resource.createSVGImage(svg, 1, 0.5, 0.5);
        circleCache[radius] = img;
    }
    gfx.drawImage(img, x, y, radius);
}

function randFreq(base, low, high) {
    if (Array.isArray(low)) {
        return base * Math.pow(2, low[Math.floor(Math.random() * low.length)] / 12.0);
    }
    return base * Math.pow(2, (Math.floor(Math.random() * (high - low + 1)) + low) / 12.0);
}

function drawFancyNumber(gfx, x, y, sc, number) {
    const ciphers =
        "*** **  *** *** * * *** *   *** *** *** " +
        "* *  *    *   * * * *   *     * * * * * " +
        "* *  *  *** *** *** *** ***   * *** *** " +
        "* *  *  *     *   *   * * *   * * *   * " +
        "***  *  *** ***   * *** ***   * ***   * ";
    const stride = ciphers.length / 5;
    const buf = String(number);
    for (let i = 0; i < buf.length; i++) {
        const digit = Number(buf[i]);
        if (!isNaN(digit)) {
            for (let posY = 0; posY < 5; posY++) {
                for (let posX = 0; posX < 4; posX++) {
                    const index = 4 * digit + posX + stride * posY;
                    if (ciphers[index] !== ' ') {
                        gfx.fillRect(x + posX * sc, y + posY * sc, sc - 1, sc - 1);
                    }
                }
            }
        }
        x += 4 * sc;
    }
}

function randColor() {
    const r = Math.floor(Math.random() * 170) + 85;
    const g = Math.floor(Math.random() * 170) + 85;
    const b = 512 - r - g;
    return (r << 24) | (g << 16) | (b << 8) | 255;
}

const highscorePath = "highscore.dat";
function saveHighscore(score) {
    resource.setStorageItem(highscorePath, String(score));
}
function loadHighscore() {
    const contents = resource.getStorageItem(highscorePath);
    return contents ? Number(contents) : 0;
}

const TYPE_PLAYER = 1;
const TYPE_BALL = 2;

class GameObject {
    constructor(x, y, radius, vx, vy, color, type) {
        this.x = x || 0;
        this.y = y || 0;
        this.radius = radius || 0;
        this.vx = vx || 0;
        this.vy = vy || 0;
        this.prevX = 0;
        this.prevY = 0;
        this.color = color || 0xFFFFFFFF;
        this.type = type || 0;
    }
    draw(gfx) {
        if (this.type === 0) return;
        gfx.color(this.color);
        fillCircle(gfx, ox + this.x, oy + this.y, this.radius);
    }
    move(x, y) {
        if (this.type === 0) return;
        this.prevX = this.x;
        this.prevY = this.y;
        this.x = x;
        this.y = y;
    }
    pan() {
        return 2.0 * this.x / vpSz - 1.0;
    }
    update(deltaT) {
        if (this.type !== TYPE_BALL) return true;
        this.move(this.x + this.vx * deltaT, this.y + this.vy * deltaT);
        if ((this.x < this.radius && this.vx < 0) || (this.x >= vpSz - this.radius && this.vx > 0)) {
            this.vx = -this.vx;
            if (this.y > this.radius) {
                beeper.beep(randFreq(220, -2, 2), 0.015, 0.3, 0.5, this.pan());
            }
        }
        if (this.y <= -this.radius && this.vy < 0) {
            return false;
        }
        return this.y < vpSz + this.radius;
    }
    intersects(other) {
        if (this.type === 0 || other.type === 0) return false;
        const dx = other.x - this.x;
        const dy = other.y - this.y;
        return (dx * dx + dy * dy) < Math.pow(this.radius + other.radius, 2);
    }
    resolveCollision(other) {
        const dx = other.x - this.x;
        const dy = other.y - this.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist === 0 || (this.fixed && other.fixed)) return;
        const nx = dx / dist;
        const ny = dy / dist;
        const tx = -ny;
        const ty = nx;
        const v1n = this.vx * nx + this.vy * ny;
        const v1t = this.vx * tx + this.vy * ty;
        const v2n = other.vx * nx + other.vy * ny;
        const v2t = other.vx * tx + other.vy * ty;
        const m1 = other.fixed ? 0 : Math.pow(this.radius, 2);
        const m2 = this.fixed ? 0 : Math.pow(other.radius, 2);
        const v1nAfter = (v1n * (m1 - m2) + 2 * m2 * v2n) / (m1 + m2);
        const v2nAfter = (v2n * (m2 - m1) + 2 * m1 * v1n) / (m1 + m2);
        this.vx = v1nAfter * nx + v1t * tx;
        this.vy = v1nAfter * ny + v1t * ty;
        other.vx = v2nAfter * nx + v2t * tx;
        other.vy = v2nAfter * ny + v2t * ty;
        const overlap = this.radius + other.radius - dist;
        if (overlap > 0) {
            const correctionFactor = overlap / (m1 + m2);
            this.x -= nx * correctionFactor * m2;
            this.y -= ny * correctionFactor * m2;
            other.x += nx * correctionFactor * m1;
            other.y += ny * correctionFactor * m1;
        }
    }
}

const Game = {
    nextLevel() {
        level++;
        bgColor = randColor();
        for (let i = 0; i < level; i++) {
            objs.push(new GameObject(
                Math.random() * vpSz,
                Math.random() * -8 * sc,
                sc * 0.5,
                Math.random() * 4 * sc - 2 * sc,
                Math.random() * 2 * sc + 2 * sc,
                0xff,
                TYPE_BALL
            ));
        }
    },
    over(restart) {
        if (score > highScore) {
            highScore = score;
            saveHighscore(highScore);
        }
        if (!restart) {
            return switchScene(require('./menu.js'));
        }
        score = 0;
        level = 0;
        players = [];
        for (let i = 0; i < numPlayers; i++) {
            players.push(new GameObject(vpSz/2, vpSz/2, sc, 0, 0, 0xFFffFFff, TYPE_PLAYER));
        }
        objs = [];
        for (const v of players) {
            objs.push(v);
        }
        Game.nextLevel();
    },
    enter(args) {
        if (args && args.players) {
            numPlayers = Number(args.players) || 1;
        }
        const winSzX = window.width();
        const winSzY = window.height();
        vpSz = Math.min(winSzX, winSzY);
        ox = (winSzX - vpSz) / 2;
        oy = (winSzY - vpSz) / 2;
        sc = vpSz / 30;
        title = resource.getImage("BALLATTAX.svg", vpSz/512, 0.5, 1);
        highScore = loadHighscore();
        Game.over(true);
    },
    update(dt, input) {
        beeper.update(dt);
        if (delay > 0) {
            delay -= dt;
            if (delay <= 0) {
                delay = 0;
                Game.over(true);
            }
            return true;
        }
        for (let i = 0; i < players.length; i++) {
            const player = players[i];
            if (i < input.length) {
                if (input[i].buttons[6] === 1 && input[i].buttons[7] === 1) {
                    Game.over(false);
                }
                player.vx = input[i].axes[0] * velMax * sc;
                player.vy = input[i].axes[1] * velMax * sc;
            }
        }
        for (const player of players) {
            player.move(player.x + player.vx * dt, player.y + player.vy * dt);
            if (player.x < player.radius) player.x = player.radius;
            else if (player.x > vpSz - player.radius) player.x = vpSz - player.radius;
            if (player.y < player.radius) player.y = player.radius;
            else if (player.y > vpSz - player.radius) player.y = vpSz - player.radius;
        }
        for (let i = objs.length - 1; i >= 0; i--) {
            const obj = objs[i];
            if (obj.type === TYPE_BALL && !obj.update(dt)) {
                if (obj.vy < 0) {
                    score++;
                    const freq = 880;
                    beeper.play({ freq, duration: 0.05, vol: 0.1, pan: obj.pan() }, { freq: 2 * freq, deltaT: 0.05 });
                    objs.splice(i, 1);
                    if (objs.length === players.length) {
                        Game.nextLevel();
                    }
                } else {
                    beeper.beep(55, 0.5, 0.3, 0.5, 0.0);
                    delay = 0.5;
                }
                break;
            }
            for (let j = 0; j < i; j++) {
                if (objs[i].intersects(objs[j])) {
                    objs[i].resolveCollision(objs[j]);
                    if (objs[i].y > 0) {
                        if (objs[i].type === TYPE_BALL && objs[j].type === TYPE_BALL) {
                            beeper.beep(randFreq(880, -3, 3), 0.015, 0.2, 0.5, objs[i].pan());
                        } else if (objs[i].type === TYPE_BALL || objs[j].type === TYPE_BALL) {
                            beeper.beep(randFreq(110, -1, 3), 0.025, 0.3, 0.5, objs[i].pan());
                        }
                    }
                }
            }
        }
        return true;
    },
    draw(gfx) {
        gfx.color(bgColor);
        gfx.fillRect(ox, oy, vpSz, vpSz);
        gfx.color(0xFFffFF55);
        gfx.drawImage(title, ox + vpSz / 2, oy + vpSz);
        gfx.color(0xFFffFF55);
        drawFancyNumber(gfx, ox + vpSz * 0.333 - 2.5 * sc, oy + 2 * sc, sc, highScore);
        gfx.color(0xFFffFFaa);
        drawFancyNumber(gfx, ox + vpSz * 0.667 - 2.5 * sc, oy + 2 * sc, sc, score);
        for (const obj of objs) {
            obj.draw(gfx);
        }
    }
};

module.exports = Game;
