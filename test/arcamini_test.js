let img = resource.getImage("test.png");
let font = resource.getFont("Viafont.ttf", 32);
let sample = resource.getAudio("ding.wav");
function createBeep(freq, dur, vol = 1.0) {
    let data = [];
    for (let n = 0; n < Math.floor(44100 * dur); n++) {
        data.push(Math.sin(2 * Math.PI * freq * n / 44100) * vol);
    }
    return resource.createAudio(data);
}
let tone880 = createBeep(880, 1.0);

resource.setStorageItem("key", "value");
let val = resource.getStorageItem("key");
console.log("Storage value:", val);

let frame = 0;

function load() {
    console.log("load called");
    console.log("window dimensions:", window.width(), window.height());
    window.color(0x000055ff);
    audio.replay(sample, 1, 0.5);
    let track = audio.replay(tone880, 0.5, -0.5);
    audio.volume(track, 0.0, 1.0); // fade out over 1 second
}

function input(evt, device, id, value, value2) {
    console.log(`input(${evt}, ${device}, ${id}, ${value}, ${value2})`);
}

function update(deltaT) {
    if (frame < 2) {
        console.log(`update called with deltaT ${deltaT} at frame ${frame}`);
    }
    return true;
}

function draw(gfx) {
    gfx.color(0xFF0000FF);
    gfx.lineWidth(2.0);
    gfx.fillRect(120, 10, 100, 50);
    gfx.drawRect(230, 10, 100, 50);
    gfx.drawLine(0, 0, 100, 100);
    gfx.fillText(font, 20, 120, "Hello", 0);
    gfx.color(0xFFffFFff);
    gfx.drawImage(img, 50, 200);
    if (frame < 2) {
        console.log("draw called at frame", frame);
    }
    frame += 1;
}

function unload() {
    console.log("unload called");
}
