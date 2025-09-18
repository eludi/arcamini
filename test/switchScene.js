let now = 0.0;

function enter(arg) {
    console.log('enter', arg);
}

function update(deltaT) {
    now += deltaT;
    if (now > 3.0) {
        window.switchScene('switchSceneDest.js', 'current_time', now);
    }
    return true;
}

function draw(gfx) {
    gfx.color(0xFF5555FF);
    gfx.fillText(0, window.width()/2, window.height()/2, 'original main file', 1);
}
