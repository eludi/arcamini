"use strict";

// arcamini browser runtime bindings.
// Implements the same JS-facing API as the native QuickJS runtime
// (see arcamini_api.md / bindings_arcaqjs.c): global lifecycle callbacks
// enter/input/update/draw/leave defined by the game script, and global
// window/gfx/audio/resource namespaces + breakpoint() provided here.
let app = arcamini.app = (function(canvas_id='arcamini_canvas') {
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
	function loadScene(fname, args) {
		if(typeof window.leave === 'function') {
			try { window.leave(); } catch(err) { reportError(err); }
		}
		window.enter = window.input = window.update = window.draw = window.leave = undefined;

		return fetch(fname).then((resp)=>{
			if(!resp.ok)
				throw new ReferenceError('window.switchScene: file not found: ' + fname);
			return resp.text();
		}).then((text)=>{
			// make sure window.width()/height() already reflect the real
			// canvas size before the script runs, since games commonly
			// read them once at top level (as falling_blocks.js does)
			adjustCanvasSize();
			// inject as a classic script so top-level function declarations
			// become real globals, exactly like the native global script eval.
			// The sourceURL comment gives the injected script a real identity in
			// DevTools (Sources panel, stack traces, breakpoints by filename)
			// instead of showing up as an anonymous <anonymous>/VM script, and
			// keeps it visibly separate from the framework files (app.js etc.)
			// that were loaded via <script src> and already have real names.
			const node = document.createElement('script');
			node.textContent = text + '\n//# sourceURL=' + fname;
			document.head.appendChild(node);
			document.head.removeChild(node);
			// mirror the native runtime's synchronous resource loading: don't fire
			// enter() until every resource requested while evaluating the script
			// (getImage/getAudio/getFont/... called at top level) is ready
			return new Promise((resolve)=>{ waitForResources(resolve); });
		}).then(()=>{
			if(typeof window.enter === 'function')
				window.enter(args);
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
		if(typeof window.input === 'function') {
			try { window.input('button', device, button, value, undefined); }
			catch(err) { reportError(err); stopApp(); }
		}
	}
	function dispatchAxisEvent(device, axis, value) {
		if(typeof window.input === 'function') {
			try { window.input('axis', device, axis, value, undefined); }
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
		if(typeof window.leave === 'function') {
			try { window.leave(); } catch(err) { reportError(err); }
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

		let keepRunning = true;
		if(typeof window.update === 'function') {
			try { keepRunning = window.update(deltaT); }
			catch(err) { reportError(err); keepRunning = false; }
		}
		if(!keepRunning) {
			stopApp();
			return;
		}

		gfxImpl._frameBegin(clearColor[0], clearColor[1], clearColor[2]);
		let drawFailed = false;
		if(typeof window.draw === 'function') {
			try { window.draw(window.gfx); }
			catch(err) { reportError(err); drawFailed = true; }
		}
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
