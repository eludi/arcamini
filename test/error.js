function erroneous() {
    throw new Error('This is an error for testing purposes.');
}

function enter(args) {
    window.color(0x0055aaff);
    console.log('enter called');
    if (Math.random() < 0.5) {
        erroneous();
    }
}

function update(deltaT) {
    console.log('update', deltaT);
    if (Math.random() < 0.5) {
        if (Math.random() < 0.5) {
            erroneous();
        } else {
            window.color(null);
        }
    }
    return true;
}

function draw(gfx) {
    // no-op
}
