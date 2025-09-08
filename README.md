# ![](test/arcajs_icon.svg "arcajs logo") arcamini

**arcamini** is a friendly, lightweight, and multi-language runtime for developing 2D console games.

- 🚀 **Easy to learn:** About [30 core functions](arcamini_api.md) for resource management, drawing, audio, and input.
- 📦 **Easy to deploy:** Download a [precompiled single-file runtime](https://github.com/eludi/arcamini/releases), add your game logic and assets, and you're ready to go.
- 🛣️ **No artificial limitations:** arcamini is not a fantasy console with outdated specs, but a modern tool for rapid console game development.
- 👐 **Open source:** [MIT licensed](LICENSE.md).

## Multi-language Support

arcamini supports game development in [JavaScript](https://bellard.org/quickjs/), [Python](https://pocketpy.dev/), and [Lua](https://www.lua.org/), with up-to-date language standards and a unified API.

## 2D Graphics

Supports both pixel sprites and scalable vector graphics (SVG) for crisp visuals on any display.

## Console Game Focus

Primarily designed for Linux-based handheld consoles, with console-style input handling. Yet games run equally well on standard PCs.

## Documentation

- [arcamini API reference](arcamini_api.md)

## Examples

- [Render performance test](./perf/)
- [BALLATTAX](./ballattax/) game
- [Functional tests](./test/)

## Language-specific Extensions

All arcamini variants support the same core APIs. Additional features:
- **arcaqjs:** Node.js-like `require()` for CommonJS modules, `console.log()`/`.warn()`/`.error()` for output.

## License

MIT License - see [LICENSE.md](LICENSE.md) for details.