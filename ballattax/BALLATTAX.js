// BALLATTAX.js - Arcaqjs/QuickJS port of main.lua

import { switchScene, currentScene, inputs } from './scene_manager.js';
import menuScene from './menu.js';

// Lifecycle callbacks
export function enter(args = {}) {
    window.color(0x000000FF); // Set background color
    switchScene(menuScene, args);
}

export function input(evt, device, id, value, value2) {
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

export function update(deltaT) {
    if (currentScene && currentScene.update) {
        return currentScene.update(deltaT, inputs);
    }
    return false;
}

export function draw(gfx) {
    if (currentScene && currentScene.draw) {
        currentScene.draw(gfx);
    }
}
