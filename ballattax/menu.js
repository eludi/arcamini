// menu.js - Arcaqjs/QuickJS port

const beeper = require('beeper.js');
const bgColor = 0xFF4080FF;
const winSzX = window.width();
const winSzY = window.height();
const sc = Math.min(winSzX / 640, winSzY / 480);
const title = resource.getImage('BALLATTAX.svg', sc, 0.5, 0.5);
const font = resource.getFont('SpaceGrotesk-Bold.ttf', Math.floor(32 * sc));
let selIndex = -1;
const menu = [ { label: 'single player' }, { label: 'multiplayer' }, { label: 'exit' } ];
let prevInputY = 0;
let running = true;

function triggerMenuEvent(index) {
    if (index < 0) {
        return;
    } else if (index === 2) {
        running = false;
        return;
    }
    switchScene(require('./game.js'), { players: index + 1 });
}

const scene = {
    enter(args) {
        for (let i = 0; i < menu.length; i++) {
            menu[i].x = winSzX / 2;
            menu[i].y = winSzY / 2 + 40 * sc * (i + 1);
        }
    },
    update(dt, inputs) {
        if (inputs[0].buttons[0] !== 0) {
            triggerMenuEvent(selIndex);
        }
        if (prevInputY === 0 && inputs[0].axes[1] !== 0) {
            beeper.beep(inputs[0].axes[1] < 0 ? 60 : 120, 0.01, 0.2);
            selIndex += (inputs[0].axes[1] > 0) ? 1 : -1;
            if (selIndex >= menu.length) {
                selIndex = 0;
            } else if (selIndex < 0) {
                selIndex = menu.length - 1;
            }
        }
        prevInputY = inputs[0].axes[1];
        return running;
    },
    draw(gfx) {
        gfx.color(bgColor);
        gfx.fillRect(0, 0, winSzX, winSzY);
        gfx.color(0xFFFFFFFF);
        gfx.drawImage(title, winSzX / 2, winSzY * 0.3);
        for (let i = 0; i < menu.length; i++) {
            if (i === selIndex) {
                gfx.color(0xFFFFFFFF);
            } else {
                gfx.color(0xFFFFFF80);
            }
            gfx.fillText(font, menu[i].x, menu[i].y, menu[i].label, 1);
        }
    }
};

module.exports = scene;
