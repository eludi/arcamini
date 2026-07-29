"use strict";

// arcamini browser runtime bindings.
// Implements the same JS-facing API as the native QuickJS runtime
// (see arcamini_api.md / bindings_arcaqjs.c): global lifecycle callbacks
// enter/input/update/draw/leave defined by the game script, and global
// window/gfx/audio/resource namespaces + breakpoint() provided here.
let app = arcamini.app = (function(canvas_id='arcamini_canvas') {
	// Captured now, synchronously, while this classic <script> is still the
	// one executing: index.html's ?game=<dir>/ handling (a later inline
	// <script>) rewrites the page's <base href> to the active game's
	// directory, so any *relative* URL resolved after that point -- e.g. a
	// lazily-created <script src="arcapy.js">, way down in makeWasmDriver --
	// would otherwise incorrectly resolve against the game's directory
	// instead of browser_runtime/, where the WASM driver files actually
	// live regardless of which game is running.
	const frameworkBaseUrl = document.currentScript ? document.currentScript.src : document.baseURI;
	const canvas = document.getElementById(canvas_id);

	// The native runtime loads resources synchronously/blocking, so a script's
	// enter() never runs before every resource.get*()/create*() call it made at
	// top level has finished. Browsers can't block on network I/O, so instead we
	// track every such load and hold enter() back until they've all settled --
	// this is also why gfxImpl (below) is constructed with a tracked callback for
	// its own built-in default font (font handle 0), which loads the same way.
	let pendingLoads = 0, onAllResourcesLoaded = null;
	function resourceLoaded() {
		if(--pendingLoads <= 0 && onAllResourcesLoaded) {
			const cb = onAllResourcesLoaded;
			onAllResourcesLoaded = null;
			cb();
		}
	}
	function trackLoad() {
		++pendingLoads;
		return resourceLoaded;
	}
	function waitForResources(cb) {
		if(pendingLoads <= 0)
			return cb();
		const timeoutId = setTimeout(()=>{
			console.error(`arcamini: timed out waiting for ${pendingLoads} resource(s) to load; proceeding anyway`);
			onAllResourcesLoaded = null;
			cb();
		}, 8000);
		onAllResourcesLoaded = ()=>{ clearTimeout(timeoutId); cb(); };
	}

	// gfxImpl is the internal rendering/resource backend (also used by resource.*
	// below); the public `gfx` global exposes exactly the documented gfx.* surface,
	// nothing more, so game scripts can't rely on anything the native runtime lacks.
	const gfxImpl = new arcamini.Graphics(canvas, 500, trackLoad());
	window.gfx = {
		color: (c)=> gfxImpl.color(c),
		lineWidth: (w)=> gfxImpl.lineWidth(w),
		transform: (x, y, rot=0, sc=1.0)=> gfxImpl.transform(x, y, rot, sc),
		save: ()=> gfxImpl.save(),
		restore: ()=> gfxImpl.restore(),
		clipRect: (x, y, w, h)=> {
			if(w < 0 || h < 0)
				gfxImpl.clipRect();
			else
				gfxImpl.clipRect(x, y, w, h);
		},
		fillRect: (x, y, w, h)=> gfxImpl.fillRect(x, y, w, h),
		drawRect: (x, y, w, h)=> gfxImpl.drawRect(x, y, w, h),
		drawLine: (x0, y0, x1, y1)=> gfxImpl.drawLine(x0, y0, x1, y1),
		drawImage: (image, x, y, rot=0, sc=1.0, flip=0)=> gfxImpl.drawImage(image, x, y, rot, sc, flip),
		fillText: (font, x, y, str, align=0)=> gfxImpl.fillText(x, y, str, font, align),
	};

	let clearColor = [0, 0, 0];
	let running = false;
	let tLastFrame = 0;
	const numControllers = 2; // reserved virtual controller slots for keyboard emulation
	const keyDeviceArrows = numControllers + 0, keyDeviceWasd = numControllers + 1;
	const gamepadResolution = 0.1;
	let gamepadStates = [];
	const btnState = {}; // per-device bitmask, for close-on-6+7 detection
	const storagePrefix = 'arcamini:' + location.pathname + ':';

	function reportError(err) {
		console.error(err);
		const msg = (err && err.message) ? err.message : String(err);
		const stack = (err && err.stack) ? ('\n' + err.stack) : '';
		alert('Error: ' + msg + stack);
	}

	function urlArgs() {
		const idx = location.href.indexOf('?');
		if(idx < 0)
			return [];
		const args = new URLSearchParams(location.href.substr(idx + 1)).get('args');
		return args ? args.split(',').map(decodeURIComponent) : [];
	}

	//--- window --------------------------------------------------------
	window.title = function(str) { document.title = str; };
	window.width = function() { return canvas.width; };
	window.height = function() { return canvas.height; };
	window.color = function(color) {
		if(color === undefined || color === 0)
			throw new TypeError("window.color(value) expects a non-zero integer color value");
		clearColor[0] = ((color >>> 24) & 0xff) / 255;
		clearColor[1] = ((color >>> 16) & 0xff) / 255;
		clearColor[2] = ((color >>> 8) & 0xff) / 255;
	};
	window.switchScene = function(script, ...args) {
		// a scene that fails to load/enter is fatal, matching the native runtime
		// (window.switchScene -> handleException -> WindowEmitClose -> leave() -> halt)
		loadScene(script, args.map(String)).catch((err)=>{ reportError(err); stopApp(); });
	};

	//--- audio -----------------------------------------------------------
	window.audio = {
		replay: function(sample, volume=1.0, balance=0.0, detune=0.0) {
			const track = arcamini.audio.replay(sample, volume, balance, detune);
			return (track === undefined || track === 0xffffffff) ? undefined : track;
		},
		volume: function(track, volume, fadeTime=0.0) {
			if(fadeTime <= 0.0) {
				if(volume > 0.0)
					arcamini.audio.volume(track, volume);
				else
					arcamini.audio.stop(track);
			}
			else if(volume <= 0.0)
				arcamini.audio.fadeOut(track, fadeTime);
			else // fade-in not supported, matches native arcmAudioVolume
				arcamini.audio.volume(track, volume);
		}
	};

	//--- resource --------------------------------------------------------
	window.resource = {
		getImage: function(name, scale=1.0, centerX=0.0, centerY=0.0, filtering=1) {
			return gfxImpl.loadTexture(name, {scale, centerX, centerY, filtering}, trackLoad());
		},
		createImage: function(data, width, height, centerX=0.0, centerY=0.0, filtering=1) {
			return gfxImpl.createTexture(width, height, data, {centerX, centerY, filtering});
		},
		createSVGImage: function(svg, scale=1.0, centerX=0.0, centerY=0.0) {
			if(!svg.startsWith('<svg xmlns='))
				svg = svg.replace(/^<svg/, '<svg xmlns="http://www.w3.org/2000/svg"');
			const blob = new Blob([svg], {type: 'image/svg+xml'});
			return gfxImpl.loadTexture(URL.createObjectURL(blob), {scale, centerX, centerY}, trackLoad());
		},
		getTileImage: function(parent, x, y, w, h, centerX=0.0, centerY=0.0) {
			return gfxImpl.createTileTexture(parent, x, y, w, h, {centerX, centerY}, trackLoad());
		},
		getTileGrid: function(image, tilesX, tilesY=1, borderW=0) {
			return gfxImpl.createTileTextures(image, tilesX, tilesY, borderW, undefined, trackLoad());
		},
		getAudio: function(name) {
			return arcamini.audio.load(name, undefined, trackLoad());
		},
		createAudio: function(data, numChannels=1) {
			return arcamini.audio.uploadPCM(data, numChannels);
		},
		getFont: function(name, fontSize=16) {
			const suffix = name.substr(name.lastIndexOf('.') + 1).toLowerCase();
			if(suffix === 'ttf')
				return gfxImpl.loadFont(name, {size: fontSize}, trackLoad());
			return gfxImpl.createImageFontResource(name, {}, trackLoad());
		},
		queryImage: function(image, property) {
			const info = gfxImpl.queryTexture(image);
			if(!info || (property !== 'width' && property !== 'height'))
				throw new TypeError(`resource.queryImage(${image}, '${property}') failed: invalid image handle or unrecognized property`);
			return info[property];
		},
		queryAudio: function(sample, property) {
			const buf = arcamini.audio.sampleBuffer(sample);
			if(buf) {
				if(property === 'channels') return buf.numberOfChannels;
				if(property === 'frames') return buf.length;
				if(property === 'sampleRate') return buf.sampleRate;
			}
			throw new TypeError(`resource.queryAudio(${sample}, '${property}') failed: invalid audio handle or unrecognized property`);
		},
		queryFont: function(font, property, str="M") {
			const m = gfxImpl.measureText(font, str);
			if(m) {
				if(property === 'width') return m.width;
				if(property === 'height') return m.height;
				if(property === 'ascent') return m.fontBoundingBoxAscent;
				if(property === 'descent') return m.fontBoundingBoxDescent;
			}
			throw new TypeError(`resource.queryFont(${font}, '${property}') failed: invalid font handle or unrecognized property`);
		},
		getStorageItem: function(key) {
			const v = localStorage.getItem(storagePrefix + key);
			return v === null ? null : v;
		},
		setStorageItem: function(key, value) {
			localStorage.setItem(storagePrefix + key, String(value));
		}
	};

	//--- debugging ---------------------------------------------------------
	window.breakpoint = function(...args) {
		console.log('breakpoint', args);
		debugger;
	};

	//--- scene lifecycle ---------------------------------------------------
	// Every scripting language plugs into the same fetch/wait-for-resources/
	// enter() sequencing below via this small driver interface -- only how a
	// script's enter/input/update/draw/leave get invoked differs per language.
	let jsModule = {}; // the currently active JS scene's module namespace
	let jsLoadCounter = 0;
	const jsDriver = {
		load: function(text, fname) {
			// Real dynamic import(), matching the native runtime's move to ES
			// modules (bindings_arcaqjs.c): enter/input/update/draw/leave are
			// read from the module's exports, not the global object, and any
			// `import ... from './helper.js'` inside the game script is
			// resolved natively by the browser -- no custom loader needed
			// here at all, unlike the WASM-hosted Python/Lua interpreters.
			// `text` (already fetched by loadScene for the file-not-found
			// check shared across all three drivers) isn't used here; the
			// browser fetches/caches the module itself as part of import().
			//
			// The query-string suffix forces a *fresh* module instance on
			// every load, matching native's switchScene() always
			// re-evaluating the entry script from scratch even for a
			// filename already seen. Nested imports inside the loaded
			// module aren't affected by this and resolve/cache normally --
			// matching native's dependency-module caching within one VM.
			return import('./' + fname + '?_t=' + (++jsLoadCounter)).then((mod)=>{
				jsModule = mod;
			});
		},
		callEnter: function(args) {
			if(typeof jsModule.enter === 'function')
				jsModule.enter(args);
		},
		callInput: function(evt, dev, id, val, val2) {
			if(typeof jsModule.input === 'function')
				jsModule.input(evt, dev, id, val, val2);
		},
		callUpdate: function(dt) {
			// no update() defined -> stop, matching the native runtime
			// (dispatchUpdateEvent defaults to false when update isn't a function)
			return typeof jsModule.update === 'function' ? jsModule.update(dt) : false;
		},
		callDraw: function() {
			if(typeof jsModule.draw === 'function')
				jsModule.draw(window.gfx);
		},
		callLeave: function() {
			if(typeof jsModule.leave === 'function')
				jsModule.leave();
		}
	};

	// makeWasmDriver: shared shape for any language compiled to WASM via the
	// bindings.h dispatcher contract (initVM/shutdownVM/dispatchLifecycleEvent
	// (Argv)/dispatchUpdateEvent/dispatchDrawEvent/dispatchAxisEvent/
	// dispatchButtonEvent -- see bindings_arcapy_wasm.c and
	// bindings_arcalua_wasm.c). Its ~30 window.*/gfx.*/audio.*/resource.*
	// bindings call straight back into the exact same window.gfx/
	// window.audio/window.resource objects above, so all of the
	// resource-readiness (trackLoad/waitForResources) and error-reporting
	// (reportError) machinery already built for JS applies for free -- no
	// separate implementation needed on that side.
	function makeWasmDriver(scriptSrc, factoryName) {
		let Module = null, modulePromise = null, vm = 0;

		function loadModule() {
			if(modulePromise)
				return modulePromise;
			modulePromise = new Promise((resolve, reject)=>{
				const node = document.createElement('script');
				node.src = new URL(scriptSrc, frameworkBaseUrl).href;
				node.onload = ()=>{ window[factoryName]().then(resolve, reject); };
				node.onerror = ()=> reject(new Error('failed to load ' + scriptSrc));
				document.head.appendChild(node);
			});
			return modulePromise;
		}
		function marshalArgs(args) {
			const ptrs = args.map((s)=> Module.allocateUTF8(String(s)));
			const arr = Module._malloc(Math.max(1, ptrs.length) * 4);
			ptrs.forEach((p, i)=> Module.setValue(arr + i * 4, p, 'i32'));
			return {arr, ptrs};
		}
		function freeArgs(m) {
			m.ptrs.forEach((p)=> Module._free(p));
			Module._free(m.arr);
		}

		return {
			load: function(text, fname) {
				return loadModule().then((mod)=>{
					Module = mod;
					if(vm) {
						Module.ccall('shutdownVM', null, ['number'], [vm]);
						vm = 0;
					}
					vm = Module.ccall('initVM', 'number', ['string', 'string'], [text, fname]);
					if(!vm)
						throw new Error('failed to evaluate ' + fname);
				});
			},
			callEnter: function(args) {
				const m = marshalArgs(args);
				try {
					Module.ccall('dispatchLifecycleEventArgv', 'boolean',
						['string', 'number', 'number', 'number'], ['enter', args.length, m.arr, vm]);
				} finally {
					freeArgs(m);
				}
			},
			callInput: function(evt, dev, id, val, val2) {
				const fn = (evt === 'axis') ? 'dispatchAxisEvent' : 'dispatchButtonEvent';
				Module.ccall(fn, null, ['number', 'number', 'number', 'number'], [dev, id, val, vm]);
			},
			callUpdate: function(dt) {
				return Module.ccall('dispatchUpdateEvent', 'boolean', ['number', 'number'], [dt, vm]);
			},
			callDraw: function() {
				Module.ccall('dispatchDrawEvent', 'boolean', ['number'], [vm]);
			},
			callLeave: function() {
				Module.ccall('dispatchLifecycleEvent', 'boolean', ['string', 'number'], ['leave', vm]);
			}
		};
	}

	// pyDriver: Python via a pocketpy interpreter compiled to WASM.
	const pyDriver = makeWasmDriver('arcapy.js', 'createArcapyModule');
	// luaDriver: Lua via a minilua interpreter compiled to WASM.
	const luaDriver = makeWasmDriver('arcalua.js', 'createArcaluaModule');

	const languageDrivers = { js: jsDriver, py: pyDriver, lua: luaDriver };
	let currentDriver = null;

	function loadScene(fname, args) {
		if(currentDriver) {
			try { currentDriver.callLeave(); } catch(err) { reportError(err); }
		}
		const ext = fname.slice(fname.lastIndexOf('.') + 1).toLowerCase();
		const driver = languageDrivers[ext];
		if(!driver)
			return Promise.reject(new Error(`window.switchScene: unsupported script type ".${ext}" for "${fname}"`));

		return fetch(fname).then((resp)=>{
			if(!resp.ok)
				throw new ReferenceError('window.switchScene: file not found: ' + fname);
			return resp.text();
		}).then((text)=>{
			// make sure window.width()/height() already reflect the real
			// canvas size before the script runs, since games commonly
			// read them once at top level (as falling_blocks.js does)
			adjustCanvasSize();
			return driver.load(text, fname);
		}).then(()=>{
			// mirror the native runtime's synchronous resource loading: don't fire
			// enter() until every resource requested while evaluating the script
			// (getImage/getAudio/getFont/... called at top level) is ready
			return new Promise((resolve)=>{ waitForResources(resolve); });
		}).then(()=>{
			currentDriver = driver;
			driver.callEnter(args);
		});
	}

	//--- input dispatch ------------------------------------------------
	function checkCloseButtons(device, button, value) {
		if(button > 15)
			return;
		let mask = btnState[device] || 0;
		if(value !== 0)
			mask |= (1 << button);
		else
			mask &= ~(1 << button);
		btnState[device] = mask;
		if((mask & (1 << 6)) && (mask & (1 << 7)))
			stopApp();
	}

	// any lifecycle callback exception is fatal in the native runtime (handleException
	// -> WindowEmitClose -> the loop stops and leave() runs); input() is no exception
	function dispatchButtonEvent(device, button, value) {
		checkCloseButtons(device, button, value);
		if(currentDriver) {
			try { currentDriver.callInput('button', device, button, value, undefined); }
			catch(err) { reportError(err); stopApp(); }
		}
	}
	function dispatchAxisEvent(device, axis, value) {
		if(currentDriver) {
			try { currentDriver.callInput('axis', device, axis, value, undefined); }
			catch(err) { reportError(err); stopApp(); }
		}
	}

	function handleKey(code, down) {
		const v = down ? 1.0 : 0.0;
		switch(code) {
		case 'ArrowLeft': dispatchAxisEvent(keyDeviceArrows, 0, down ? -1.0 : 0.0); break;
		case 'ArrowRight': dispatchAxisEvent(keyDeviceArrows, 0, down ? 1.0 : 0.0); break;
		case 'ArrowUp': dispatchAxisEvent(keyDeviceArrows, 1, down ? -1.0 : 0.0); break;
		case 'ArrowDown': dispatchAxisEvent(keyDeviceArrows, 1, down ? 1.0 : 0.0); break;
		case 'Space':
		case 'Enter': dispatchButtonEvent(keyDeviceArrows, 0, v); break;
		case 'Backspace': dispatchButtonEvent(keyDeviceArrows, 1, v); break;
		case 'AltRight': dispatchButtonEvent(keyDeviceArrows, 2, v); break;
		case 'ControlRight': dispatchButtonEvent(keyDeviceArrows, 3, v); break;
		case 'Tab': dispatchButtonEvent(keyDeviceArrows, 6, v); break;
		case 'Escape': dispatchButtonEvent(keyDeviceArrows, 7, v); break;

		case 'KeyA': dispatchAxisEvent(keyDeviceWasd, 0, down ? -1.0 : 0.0); break;
		case 'KeyD': dispatchAxisEvent(keyDeviceWasd, 0, down ? 1.0 : 0.0); break;
		case 'KeyW': dispatchAxisEvent(keyDeviceWasd, 1, down ? -1.0 : 0.0); break;
		case 'KeyS': dispatchAxisEvent(keyDeviceWasd, 1, down ? 1.0 : 0.0); break;
		case 'Digit1': dispatchButtonEvent(keyDeviceWasd, 0, v); break;
		case 'Digit2': dispatchButtonEvent(keyDeviceWasd, 1, v); break;
		case 'Digit3': dispatchButtonEvent(keyDeviceWasd, 2, v); break;
		case 'Digit4': dispatchButtonEvent(keyDeviceWasd, 3, v); break;
		}
	}

	function getGamepad(index) {
		const pads = navigator.getGamepads ? navigator.getGamepads() : [];
		const pad = pads[index];
		if(!pad || !pad.connected)
			return {index, connected: false};
		const ret = {index, connected: true, buttons: [], axes: []};
		for(let i=0; i<pad.buttons.length; ++i)
			ret.buttons[i] = pad.buttons[i].pressed;
		for(let i=0; i<pad.axes.length; ++i)
			ret.axes[i] = pad.axes[i];
		return ret;
	}

	function pollGamepads() {
		const pads = navigator.getGamepads ? navigator.getGamepads() : [];
		for(let index=0; index<pads.length; ++index) {
			const pad = pads[index];
			if(!pad || !pad.connected) {
				delete gamepadStates[index];
				continue;
			}
			let state = gamepadStates[index];
			if(!state) { // just connected: seed state, don't emit spurious events
				gamepadStates[index] = {
					axes: pad.axes.map(()=>0),
					buttons: pad.buttons.map(()=>false)
				};
				continue;
			}
			for(let i=0; i<pad.buttons.length; ++i) {
				const pressed = pad.buttons[i].pressed;
				if(pressed !== state.buttons[i]) {
					state.buttons[i] = pressed;
					dispatchButtonEvent(index, i, pressed ? 1.0 : 0.0);
				}
			}
			for(let i=0; i<pad.axes.length; ++i) {
				let v = pad.axes[i];
				if(Math.abs(v) < gamepadResolution)
					v = 0;
				if(Math.round(v/gamepadResolution) !== Math.round(state.axes[i]/gamepadResolution)) {
					state.axes[i] = v;
					dispatchAxisEvent(index, i, v);
				}
			}
		}
	}

	window.addEventListener('keydown', (evt)=>{
		if(evt.repeat)
			return;
		handleKey(evt.code, true);
		evt.preventDefault();
	});
	window.addEventListener('keyup', (evt)=>{
		handleKey(evt.code, false);
	});
	// mirror the native runtime's "closing the application window" trigger for leave()
	window.addEventListener('beforeunload', stopApp);

	//--- main loop -------------------------------------------------------
	function adjustCanvasSize() {
		const width = canvas.clientWidth, height = canvas.clientHeight;
		if(canvas.width !== width || canvas.height !== height) {
			canvas.width = width;
			canvas.height = height;
		}
	}

	function stopApp() {
		if(!running)
			return;
		running = false;
		if(currentDriver) {
			try { currentDriver.callLeave(); } catch(err) { reportError(err); }
		}
	}

	function mainLoop(now) {
		if(!running)
			return;
		now *= 0.001;
		const deltaT = now - tLastFrame;
		tLastFrame = now;

		pollGamepads();
		adjustCanvasSize();

		let keepRunning = false;
		try { keepRunning = currentDriver.callUpdate(deltaT); }
		catch(err) { reportError(err); keepRunning = false; }
		if(!keepRunning) {
			stopApp();
			return;
		}

		gfxImpl._frameBegin(clearColor[0], clearColor[1], clearColor[2]);
		let drawFailed = false;
		try { currentDriver.callDraw(); }
		catch(err) { reportError(err); drawFailed = true; }
		gfxImpl._frameEnd();
		if(drawFailed) {
			stopApp();
			return;
		}

		requestAnimationFrame(mainLoop);
	}

	function runLoop() {
		if(running)
			return;
		running = true;
		tLastFrame = performance.now() * 0.001;
		requestAnimationFrame(mainLoop);
	}

	//--- public app helpers used by index.html's bootstrap ---------------
	return {
		args: urlArgs(),
		// exposed so non-JS language drivers (compiled to WASM, calling back
		// into JS) can surface an interpreter-level exception exactly like a
		// JS lifecycle-callback exception is reported
		_reportError: reportError,
		fullscreen: function(fullscreen) {
			if(fullscreen) {
				if(document.fullscreenEnabled && !document.fullscreenElement)
					document.documentElement.requestFullscreen();
				else if(document.webkitFullscreenEnabled && !document.webkitFullscreenElement)
					document.documentElement.webkitRequestFullscreen();
			}
			else if(document.fullscreenElement)
				document.exitFullscreen();
			else if(document.webkitFullscreenElement)
				document.webkitExitFullscreen();
		},
		_getGamepad: getGamepad,
		_boot: function(fname, args) {
			// a failed enter() must not start the loop, matching the native runtime's
			// `if(dispatchLifecycleEventArgv("enter", ...)) { while(...) ... }` gating
			loadScene(fname, args).then(runLoop).catch(reportError);
		}
	};
})();
