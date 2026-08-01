export function enter(args) {
    window.color(0x202020ff);
}

export function draw(gfx) {
    gfx.color(0x40c0ffff);
    gfx.fillRect(window.width() / 2 - 25, window.height() / 2 - 25, 50, 50);
}
