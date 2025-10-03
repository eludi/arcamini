now = 0.0;
console.log('yay, switchScene worked!');

function enter(args) {
    console.log('enter called with args:', args);
}

function input(evt, device, id, value, value2) {
    if (evt == 'button' && value == 1)
        window.switchScene("switchScene.js", "back", "to", "the", "first", "scene");
}

function update(deltaT) {
    now += deltaT;
    return true;
}

function draw(gfx) {
    gfx.color(now % 1.0 < 0.5 ? 0xFFFFFFFF : 0x55FF55FF);
    gfx.fillText(0, window.width()/2, window.height()/2, 'yay, switchScene worked!', 1);
}

function leave() {
    console.log('leave called');
}
