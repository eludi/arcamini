# ![](test/arcajs_icon.svg "arcajs logo") arcamini

the friendly and lightweight multi-language 2D console game runtime.

Wait, what do you mean by friendly?
- easy to learn. Just about [30 functions](arcamini_api.md) providing you the essentials of
  resource loading or generation, drawing, audio, and input handling
- easy to run and deploy. Pick one of the small runtimes precompiled as single file, add
  your game logic and assets, and you are ready to go.
- no artificial limitations. Arcamini is not a fantasy console mimicking specs of devices
  from long ago, but a modern tool making console development easy and fun.
- Open source and [permissively licensed](LICENSE.md).

Did you say multi-language?
- yes, arcamini can be used to develop games in Javascript, Python, and Lua. All variants offer
  the same feature set and API.

2D...?
- 2D means both pixel sprites and vector graphics. Or, more specifically, vector-based pixel sprites.
  A distinctive feature of arcamini is that it supports SVG scalable vector graphics as sprite format,
  so your games can look equally crisp and awesome on tiny handhelds as well as on huge displays.

Why *console* game runtime?
- arcamini's feature set is specifically tailored towards Linux-based handheld game console
  development. This is why its input handling is always console-like. However, since such
  games are never developed on the device itself but on normal PCs, the games run fine there
  as well.

## Documentation

- [arcamini API reference](arcamini_api.md)

### Examples
  - [render performance test](./perf/)
  - [BALLATTAX](./ballattax/) game
  - [functional tests](./test/)

### Language-specific APIs
Generally, all arcamini support the same arcamini core APIa and all the default modules and features provided by the respective language runtimes [QucikJS](), [pocketpy](), and [Lua5.4](). For details, refer to their respective documentation at their websites.

Besides that, some arcamini variants provide a few language-specific extensions
- arcaqjs
  - require() global function, node.js-like CommonJS module import
  - console.log() /.warn() / .error() for console output

## License

This project is licensed under the permissive MIT License - see the
[LICENSE.md](LICENSE.md) file for further details.
