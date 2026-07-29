// scene_manager.js - shared scene-switching state for BALLATTAX.js/menu.js/
// game.js. Under the old require()-as-global-eval mechanism, menu.js/game.js
// could call BALLATTAX.js's switchScene() as a bare global function, since
// every require()'d file shared one global object. Real ES modules don't do
// that -- each file only sees what it explicitly imports -- so this state
// moved out into its own module that all three import, rather than having
// menu.js/game.js import from the entry script itself (which would make the
// entry module both the top-level module and a transitively-imported
// dependency of its own dependency graph).
export let currentScene = null;
export let inputs = [{axes: [0, 0], buttons: [0, 0, 0, 0, 0, 0, 0, 0]}];

export function switchScene(newScene, args) {
    if (currentScene && typeof currentScene.exit === 'function') {
        currentScene.exit();
    }
    currentScene = newScene;
    if (currentScene && typeof currentScene.enter === 'function') {
        currentScene.enter(args);
    }
}
