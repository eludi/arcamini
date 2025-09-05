// BALLATTAX.js - Arcaqjs/QuickJS port of main.lua


let currentScene;

// current input state, updated by input events
let inputs = [
    { axes: [0, 0], buttons: [0, 0, 0, 0, 0, 0, 0, 0] }
];

// Scene management, dispatch events to game.js and menu.js
function switchScene(newScene, args) {
    if (currentScene && typeof currentScene.exit === 'function') {
        currentScene.exit();
    }
    currentScene = newScene;
    if (currentScene && typeof currentScene.enter === 'function') {
        currentScene.enter(args);
    }
}

// Lifecycle callbacks
function load(args = {}) {
    window.color(0x000000FF); // Set background color
    switchScene(require(args.scene || 'menu.js'), args);
}

function input(evt,device,id,value,value2) {
    // update inputs state, evt describes a state change:
    while (device >= inputs.length) {
        inputs.push({ axes: [0, 0], buttons: [0, 0, 0, 0, 0, 0, 0, 0] });
    }
    if (evt === 'axis') {
        inputs[device].axes[id] = value;
    } else if (evt === 'button') {
        inputs[device].buttons[id] = value;
    }
    if (currentScene && currentScene.input) {
        currentScene.input(evt,device,id,value,value2);
    }
}

function update(deltaT) {
    if (currentScene && currentScene.update) {
        return currentScene.update(deltaT, inputs);
    }
    return false;
}

function draw(gfx) {
    if (currentScene && currentScene.draw) {
        currentScene.draw(gfx);
    }
}
