# ![](test/arcamini_icon.svg "arcamini logo") arcamini

**arcamini** is a friendly, lightweight, and multi-language runtime for developing 2D console games.

- 🎓 **Easy to learn:** About [30 core functions](arcamini_api.md) for resource management, drawing, audio, and input.
- 📦 **Easy to deploy:** Download a [precompiled single-file runtime](https://github.com/eludi/arcamini/releases), add your game logic and assets, and you're ready to go.
- 🚀 **No artificial limitations:** arcamini is not a fantasy console with outdated specs, but a modern framework for rapid console game development.
- 👐 **Open source:** [MIT licensed](LICENSE.md).

## Multi-language Support

arcamini supports game development in [JavaScript](https://bellard.org/quickjs/), [Python](https://pocketpy.dev/), and [Lua](https://www.lua.org/), with up-to-date language standards and a unified API.

## 2D Graphics

Supports both pixel sprites and scalable vector graphics (SVG) for crisp visuals on any display.

## Console Game Focus

Primarily designed for Linux-based handheld consoles, with console-style input handling. Yet games run equally well on standard PCs and any modern webbrowser.

## Browser Runtime

The [browser runtime](browser_runtime/) is a multi-language runtime implementing the same API as **arcamini**, backed by WebAssembly, WebGL and Web Audio. Point a web server at the directory, add your game scripts and assets, adjust the `manifest.json` scripts list,  and any arcamini game runs unmodified in a browser, no local installation or plugin required.

## Examples

- [mini games](./minigames/) - Falling Blocks ([play live](https://eludi.github.io/arcamini/browser_runtime/), JS), Box Breaker ([play live](https://eludi.github.io/arcamini/browser_runtime/?game=../minigames/&manifest=manifest_box_breaker.json), Python), Flappy Box ([play live](https://eludi.github.io/arcamini/browser_runtime/?game=../minigames/&manifest=manifest_flappy_box.json), Python), Snake ([play live](https://eludi.github.io/arcamini/browser_runtime/?game=../minigames/&manifest=manifest_snake.json), Python) - each in just about 40-60 lines of code
- [Render performance test](./perf/)
- [BALLATTAX](./ballattax/) game - play live in [JS](https://eludi.github.io/arcamini/browser_runtime/?game=../ballattax/), [Python](https://eludi.github.io/arcamini/browser_runtime/?game=../ballattax/&manifest=manifest_py.json), or [Lua](https://eludi.github.io/arcamini/browser_runtime/?game=../ballattax/&manifest=manifest_lua.json)
- [Functional tests](./test/) - run live in [JS](https://eludi.github.io/arcamini/browser_runtime/?game=../test/), [Python](https://eludi.github.io/arcamini/browser_runtime/?game=../test/&manifest=manifest_py.json), or [Lua](https://eludi.github.io/arcamini/browser_runtime/?game=../test/&manifest=manifest_lua.json)

## Online IDE

The [arcamini-IDE](ide/) is a browser-based mini IDE for writing, running, and exporting arcamini games in JavaScript, Python, or Lua — no local install required, making it a low-friction way to try arcamini or teach it in an introductory course. Write a project from scratch or fork one of the examples above, run it live against the unmodified [browser runtime](browser_runtime/), and export it as a self-contained game directory ready to deploy. [Try it live](https://eludi.github.io/arcamini/ide/).

## Documentation

- [arcamini API reference](arcamini_api.md)

## Language-specific Extensions

All arcamini variants support the same core APIs. Additional features:
- **arcaqjs:** browser-like `console.log()`/`.warn()`/`.error()` for output.

## License

MIT License - see [LICENSE.md](LICENSE.md) for details.
