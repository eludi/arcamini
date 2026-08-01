'use strict';
// IDE controller. UI/editor/project-list machinery ported and adapted from
// https://github.com/eludi/elowo/blob/main/ide.js -- see plan-arcamini-ide.md
// for the full rationale. The big structural difference from elowo: there is
// no in-page "runtime" here at all. Run navigates to the *unmodified*
// browser_runtime/index.html, after seeding Cache Storage with this
// project's manifest.json/script/resources under the URLs that page will
// naturally request -- see run() below.
window._app = (function(parent) {
let ide = Object.create(parent);
ide.applets = {};
ide.tStart = null;
ide.installEvt = null;
ide.installationRejected = false;
ide.editor = null;
ide.auxEditor = null;
ide.sidebar = null;
ide.screenStack = [];
ide.language = 'js';
ide.entryFilename = 'main.js';
ide.forkedFrom = null;

const LANG_EXT = { js:'js', py:'py', lua:'lua' };

function isIOS() { return /ipad|iphone|ipod/i.test(navigator.userAgent.toLowerCase()); }

const Editor = function(screen, title, content='') {
	this.replaceSmartPunctuation = function(str) {
		const convMap = {
			0x2018:'\'', 0x201B:'\'', 0x201C:'"', 0x201F:'"', "‚": "'", "„": '"',
			0x2019:'\'', 0x201D:'\"', "‘": "'", "“": '"',
			0x2032:'\'', 0x2033:'"', 0x2035:'\'', 0x2036:'"', 0x2014:'-',
			0x2013:'-'
		};
		for(const key in convMap)
			str = str.replace(new RegExp(key, 'g'), convMap[key]);
		return str;
	}
	this.getContent = function() {
		return this.replaceSmartPunctuation(this.textarea.value);
	}
	this.reset = function(name, content='') {
		screen.querySelector('.editor_title').value = name;
		this.textarea.value = content;
	}
	this.getCursorPos = function(textarea) {
		let lines = textarea.value.substr(0, textarea.selectionStart).split("\n");
		let pos = { x:lines[lines.length-1].length+1, y:lines.length };
		let currLine = lines[pos.y-1];
		if(!currLine && pos.y>1)
			currLine = lines[pos.y-2];
		this.currentIndent = currLine.substr(0, currLine.search(/\S/));
		return pos;
	}
	this.getState = function() {
		return { selectionStart:this.textarea.selectionStart, scrollTop:this.textarea.scrollTop };
	}
	this.setState = function(state) {
		this.textarea.selectionStart = state.selectionStart;
		this.textarea.selectionEnd = state.selectionStart;
		this.textarea.scrollTop = state.scrollTop;
	}
	this.insertAtCursor = function(input, textToInsert) {
		const isSuccess = document.execCommand("insertText", false, textToInsert);
		if (!isSuccess && typeof input.setRangeText === "function") {
			const start = input.selectionStart;
			input.setRangeText(textToInsert);
			input.selectionStart = input.selectionEnd = start + textToInsert.length;
		}
	}

	this.currentIndent = '';
	this.textarea = screen.querySelector('textarea');
	[ 'input', 'keydown', 'keyup', 'click', 'focus' ].forEach((evtName)=>{
		this.textarea.addEventListener(evtName, (e)=>{
			let pos = this.getCursorPos(this.textarea);
			screen.querySelector('.editor_pos').innerHTML = 'Ln&nbsp;'+pos.y+'<br/>Col&nbsp;'+pos.x;
		});
	});
	this.textarea.addEventListener('keydown', (evt)=>{
		let ta = this.textarea;
		if(evt.key=='Enter') {
			if(this.currentIndent)
				setTimeout(()=>{
					this.insertAtCursor(ta, this.currentIndent);
				}, isIOS() ? 25 : 0);
		}
		else if(evt.key=='Tab') {
			evt.preventDefault();
			let cursorX = this.getCursorPos(ta).x - 1;
			let numSpaces = 4 - (cursorX % 4);
			this.insertAtCursor(ta, ' '.repeat(numSpaces));
		}
		else if(evt.key=='Backspace') {
			if (ta.selectionStart != ta.selectionEnd || ta.selectionStart===0)
				return;
			let cursorX = this.getCursorPos(ta).x - 1;
			let newPos = ta.selectionStart - Math.min((cursorX % 4) || 4, cursorX);
			for(--ta.selectionStart ; ta.selectionStart>newPos; --ta.selectionStart)
				if(ta.value.charAt(ta.selectionStart)!=' ')
					break;
			if(ta.value.charAt(ta.selectionStart)!=' ')
				++ta.selectionStart;
		}
	});
	screen.querySelector('.editor_title').value = title;
	this.textarea.value = content;
};

//--- bootstrapping -----------------------------------------------------
ide.init = async function() {
	if('serviceWorker' in navigator && location.protocol != 'file:') {
		navigator.serviceWorker.register('../serviceworker.js', {scope:'../'}).then(()=>{
			console.log('service worker installed');
		}).catch(err => console.error('service worker registration failed', err));
		window.addEventListener('beforeinstallprompt', evt => {
			if(this.installEvt === null)
				this.installEvt = evt;
			evt.preventDefault();
		});
	}
	this.sidebar = new (function(elem) {
		this.close = function() { elem.className = 'closed'; }
		this.toggle = function() {
			elem.className = (elem.className=='closed') ? 'open' : 'closed';
		}
	})(document.querySelector('#editor_sidebar'));

	this.editor = new Editor(document.querySelector('#screen_editor'), this.currentApplet);
	this.metaInit();
	this.applets_init();

	for(let elems = document.querySelectorAll('button'), i=0, el; el=elems[i]; ++i)
		if(el.value)
			el.addEventListener('click', (evt)=>{
				this.handleUIEvent(evt.currentTarget.value.split(/\s+/)); });

	let editorTitle = document.querySelector('#screen_editor .editor_title');
	editorTitle.addEventListener('change', function(e) { ide.renameApplet(this.value); });

	document.querySelector('#loadFS').addEventListener('change', function(e) {
		for(let i=0; i<this.files.length; ++i) {
			let file = this.files[i];
			let reader = new FileReader();
			reader.addEventListener("load", function(e) { ide.importApplet(file, this.result); });
			reader.readAsText(file);
		}
	});
	document.querySelector('#importRes').addEventListener('change', function(e) {
		for(let i=0; i<this.files.length; ++i)
			ide.resources.import(this.files[i], true);
	});
	document.querySelector('#langSelect').addEventListener('change', function(e) {
		ide.language = this.value;
	});

	this.tStart = new Date();
	this.loadApiReference();
	// initial visibility only -- past load time this is purely manual via
	// #btn_toggleHelp ('toggleHelp' in handleUIEvent), so resizing an
	// already-open/closed window doesn't fight the user's own choice
	if(window.innerWidth >= 1200)
		document.getElementById('apiref_panel').classList.add('visible');
};

//--- API reference side panel (see index.html's #apiref_panel, toggled by
//--- #btn_toggleHelp via the 'visible' class -- see handleUIEvent)
ide.loadApiReference = async function() {
	const panel = document.getElementById('apiref_panel');
	if(!panel)
		return;
	try {
		const resp = await fetch('../arcamini_api.md');
		if(!resp.ok)
			throw new Error(resp.status);
		panel.innerHTML = this.renderMarkdown(await resp.text());
	} catch(err) {
		panel.innerHTML = '<p>API reference unavailable (' + err + ').</p>';
	}
};
// Deliberately not a general CommonMark parser -- arcamini_api.md only ever
// uses headers, bullet lists, inline code/bold and plain paragraphs, so a
// line-based pass covers it without vendoring a markdown library.
ide.renderMarkdown = function(text) {
	const esc = (s)=> s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
	const inline = (s)=> esc(s)
		.replace(/`([^`]+)`/g, '<code>$1</code>')
		.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');

	let html = '', inList = false;
	const closeList = ()=>{ if(inList) { html += '</ul>'; inList = false; } };
	for(const line of text.split('\n')) {
		const heading = line.match(/^(#{1,4})\s+(.*)$/);
		if(heading) {
			closeList();
			html += `<h${heading[1].length}>${inline(heading[2])}</h${heading[1].length}>`;
			continue;
		}
		const item = line.match(/^-\s+(.*)$/);
		if(item) {
			if(!inList) { html += '<ul>'; inList = true; }
			html += `<li>${inline(item[1])}</li>`;
			continue;
		}
		if(line.trim() === '') {
			closeList();
			continue;
		}
		closeList();
		html += `<p>${inline(line)}</p>`;
	}
	closeList();
	return html;
};

//--- project (applet) storage: plain localStorage, one JSON blob --------
ide.applets_init = function() {
	if(typeof localStorage === 'undefined')
		return this.newApplet();
	let data = localStorage.getItem('arcamini_ide_projects');
	if(data!==null) {
		try { data = JSON.parse(data); }
		catch(err) { console.error(err); data = null; }
	}
	if(typeof data === 'object' && data)
		this.applets = data;

	let state = localStorage.getItem('arcamini_ide_state');
	if(state!==null) {
		try { state = JSON.parse(state); }
		catch(err) { console.error(err); state = null; }
		if(state && typeof state==='object' && state.currentApplet in this.applets) {
			this.openApplet(state.currentApplet);
			if('editor' in state)
				this.editor.setState(state.editor);
			if(state.installationRejected)
				this.installationRejected = true;
			return;
		}
	}
	this.newApplet();
};
ide.storeCurrentApplet = function() {
	const source = this.editor.getContent();
	let data = {
		source, language:this.language, entryFilename:this.entryFilename,
		forkedFrom:this.forkedFrom
	};
	if(!this.metaEmpty())
		data.meta = this.currentMeta;
	if(!this.resources.empty())
		data.resources = this.resources.serialize();
	this.applets[this.currentApplet] = data;

	try {
		if(typeof localStorage !== 'undefined') {
			localStorage.setItem('arcamini_ide_projects', JSON.stringify(this.applets));
			let state = { currentApplet:this.currentApplet, editor: this.editor.getState() };
			if(this.installationRejected)
				state.installationRejected = true;
			localStorage.setItem('arcamini_ide_state', JSON.stringify(state));
		}
	} catch (err) { this.error(String(err)); }
	return data;
};
ide.newApplet = async function(language) {
	language = language || document.getElementById('langSelect').value || 'js';
	let newAppletName = (prefix)=>{
		let name = prefix;
		let counter = 0;
		while(name in this.applets)
			name = prefix + (++counter);
		return name;
	}
	this.storeCurrentApplet();
	this.language = language;
	this.entryFilename = 'main.' + LANG_EXT[language];
	this.forkedFrom = null;
	this.currentApplet = newAppletName('myProject');
	this.resources.reset();
	document.getElementById('langSelect').value = language;
	let source = '';
	try {
		const resp = await fetch('templates/blank.' + LANG_EXT[language]);
		if(resp.ok)
			source = await resp.text();
	} catch(err) { /* offline first run: fall back to an empty script */ }
	this.editor.reset(this.currentApplet, source);
	this.metaReset();
	this.sidebar.close();
};
ide.openApplet = function(name, data) {
	data = data || this.applets[name];
	if(!data)
		return this.error('project '+name+' does not exist');

	this.applets[name] = data;
	this.currentApplet = name;
	this.language = data.language || 'js';
	this.entryFilename = data.entryFilename || ('main.' + LANG_EXT[this.language]);
	this.forkedFrom = data.forkedFrom || null;
	document.getElementById('langSelect').value = this.language;

	this.resources.reset(data.resources, true);
	this.editor.reset(name, data.source || '');
	this.metaReset(data.meta);
	this.sidebar.close();
	this.log('project '+name+' opened');
};
ide.importApplet = function(file, data) {
	try { data = JSON.parse(data); }
	catch(err) { return this.error('invalid project file: '+err); }
	return this.openApplet(fileUtils.baseName(file.name), data);
};
ide.exportApplet = function() {
	const data = this.storeCurrentApplet();
	fileUtils.saveText(JSON.stringify(data), fileUtils.baseName(this.currentApplet)+'.json', 'application/json');
};
ide.renameApplet = function(name) {
	if(!name || name === this.currentApplet)
		return;
	delete this.applets[this.currentApplet];
	this.currentApplet = name;
	this.storeCurrentApplet();
};
ide.removeApplet = function(name) {
	delete this.applets[name];
	let ul = document.querySelector('#files_list');
	for(let item=ul.firstChild; item!==null; item=item.nextSibling)
		if(item.dataset.name == name) {
			ul.removeChild(item);
			break;
		}

	if(this.currentApplet == name)
		return this.newApplet(this.language);
	this.storeCurrentApplet();
};
ide.visualizeApplets = function(data) {
	let ul = document.querySelector('#files_list');
	while (ul.lastChild)
		ul.removeChild(ul.lastChild);

	for(let name in data) {
		let li = ul.appendChild(document.createElement('li'));
		let center = li.appendChild(document.createElement('div'));
		center.className = 'center';
		let right = li.appendChild(document.createElement('div'));

		let template = document.querySelector('#btn_delete');
		let btnDelete = right.appendChild(document.createElement('button'));
		btnDelete.innerHTML = template.innerHTML;
		btnDelete.title = template.title;
		btnDelete.addEventListener('click', (evt)=>{
			if(window.confirm('Delete '+name+'?'))
				this.removeApplet(evt.currentTarget.value);
		});

		let h = center.appendChild(document.createElement('h3'));
		btnDelete.value = li.dataset.name = h.innerText = name + '  [' + (data[name].language||'js') + ']';

		let content = data[name].source || '';
		let preview = center.appendChild(document.createElement('code'));
		preview.innerText = content.substr(0, content.indexOf('\n', 0));

		li.addEventListener('click', (evt)=>{
			let name = evt.currentTarget.dataset.name;
			this.storeCurrentApplet();
			this.openApplet(name);
			this.setScreen('editor');
		});
	}
}

//--- fork an existing example --------------------------------------------
ide.forkExample = async function() {
	const gameDir = window.prompt('Example directory (relative to browser_runtime/):', '../minigames/');
	if(!gameDir)
		return;
	const manifestFile = window.prompt('Manifest filename:', 'manifest_box_breaker.json') || 'manifest.json';
	try {
		const runtimeUrl = new URL('../browser_runtime/index.html', location.href);
		const base = new URL(gameDir, runtimeUrl);
		const manifestUrl = new URL(manifestFile, base);
		const manifest = await fetch(manifestUrl).then((r)=>{
			if(!r.ok) throw new Error('manifest not found: '+manifestUrl);
			return r.json();
		});
		const scriptName = manifest.scripts && manifest.scripts[0];
		if(!scriptName)
			throw new Error('manifest has no scripts entry');
		const scriptText = await fetch(new URL(scriptName, base)).then((r)=>{
			if(!r.ok) throw new Error('script not found: '+scriptName);
			return r.text();
		});
		const ext = scriptName.slice(scriptName.lastIndexOf('.')+1).toLowerCase();
		const language = (ext in LANG_EXT) ? ext : 'js';

		this.storeCurrentApplet();
		this.language = language;
		this.entryFilename = scriptName;
		this.forkedFrom = { gameDir, manifestFile };
		this.currentApplet = fileUtils.baseName(scriptName) + '_fork';
		document.getElementById('langSelect').value = language;
		this.resources.reset();
		this.editor.reset(this.currentApplet, scriptText);
		this.metaReset(manifest);
		this.sidebar.close();
		this.log('forked '+scriptName+' from '+gameDir);
	} catch(err) {
		this.error('fork failed: '+err);
	}
};

//--- run: seed Cache Storage, then navigate to the unmodified runtime ----
ide.buildManifest = function() {
	const m = this.currentMeta || {};
	return {
		name: m.name || this.currentApplet,
		start_url: './',
		display: m.display || 'window',
		window_width: m.window_width ? parseInt(m.window_width) : 640,
		window_height: m.window_height ? parseInt(m.window_height) : 480,
		audio_tracks: 4,
		scripts: [ this.entryFilename ],
		orientation: m.orientation || 'landscape',
		background_color: m.background_color || 'black',
		theme_color: m.theme_color || m.background_color || 'black'
	};
};
ide.run = async function() {
	this.storeCurrentApplet();
	if(!('caches' in window))
		return this.error('this browser has no Cache Storage support, cannot run in the IDE');

	const runtimeUrl = new URL('../browser_runtime/index.html', location.href);
	let gameDir, manifestFile, base;
	if(this.forkedFrom) {
		gameDir = this.forkedFrom.gameDir;
		manifestFile = this.forkedFrom.manifestFile;
		base = new URL(gameDir, runtimeUrl);
	}
	else {
		gameDir = '../ide/run/current/';
		manifestFile = 'manifest.json';
		base = new URL(gameDir, runtimeUrl);
	}

	await caches.delete('ide-run');
	const cache = await caches.open('ide-run');
	const putText = (url, text, mime) => cache.put(url, new Response(text, {headers:{'Content-Type':mime}}));

	// manifest.json is always overlaid (meta-form edits count as edits too,
	// same as the script); everything else in a forked directory that
	// wasn't touched here still falls through to the real network.
	await putText(new URL(manifestFile, base), JSON.stringify(this.buildManifest()), 'application/json');
	await putText(new URL(this.entryFilename, base), this.editor.getContent(), fileUtils.fileType(this.entryFilename) || 'text/plain');

	const resources = this.resources.serialize();
	for(let name in resources) {
		const item = resources[name];
		if(!item.url)
			continue;
		const buf = await fileUtils.dataUrlToArrayBuffer(item.url);
		await cache.put(new URL(name, base), new Response(buf, {headers:{'Content-Type':item.mime}}));
	}

	const runUrl = new URL(runtimeUrl);
	runUrl.search = '?game=' + encodeURIComponent(gameDir) +
		(manifestFile !== 'manifest.json' ? '&manifest=' + encodeURIComponent(manifestFile) : '');
	location.href = runUrl.href;
};

//--- metadata form, mapped to arcamini manifest.json fields --------------
ide.metaInit = function() {
	let elems = document.querySelectorAll('#meta_middle > input, #meta_middle > select');
	for(let i=0, el; el=elems[i]; ++i) {
		el.addEventListener('change', (evt)=>{
			let id = evt.currentTarget.id.substr(5);
			this.currentMeta[id] = evt.currentTarget.value;
		});
	}
}
ide.metaReset = function(data) {
	this.currentMeta = data = data || {};
	let parent = document.getElementById('meta_middle');
	for(let item=parent.firstChild; item!==null; item=item.nextSibling) {
		if(item.id.startsWith('meta_')) {
			let key = item.id.substr(5);
			item.value = (key in data) ? data[key] : '';
		}
	}
}
ide.metaEmpty = function() {
	for(let id in this.currentMeta)
		return false;
	return true;
}

//--- aux editor (used by resources 'terms of use' etc, kept for parity) --
ide.openAuxEditor = function(title, content, onclose) {
	let editor = this.auxEditor = new Editor(
		document.querySelector('#screen_auxEditor'), title, content);
	if(onclose)
		editor.onclose = onclose;
	this.setScreen('auxEditor');
}
ide.closeAuxEditor = function() {
	if(this.auxEditor) {
		if(this.auxEditor.onclose)
			this.auxEditor.onclose(this.auxEditor.getContent());
		this.auxEditor = null;
	}
	this.setScreen(this.screenStack.pop() || 'editor');
}
ide.toggleOvl = function(id, open) {
	let ovl = document.getElementById(id);
	if(open===undefined)
		ovl.classList.toggle('hidden');
	else if(open)
		ovl.classList.remove('hidden');
	else
		ovl.classList.add('hidden');
}
ide.createResource = function(mime) {
	this.resources.create(mime);
	this.toggleOvl('ovl_createRes', false);
}

ide.handleUIEvent = function(args) {
	if(args[0]=='screen') {
		this.setScreen(args[1]);
		for(let elems = document.querySelectorAll('.ide_ovl'), i=0, el; el=elems[i]; ++i)
			this.toggleOvl(el.id, false);
	}
	else if(args[0]=='toggleOvl')
		this.toggleOvl(args[1]);
	else if(args[0]=='run')
		this.run();
	else if(args[0]=='new')
		this.newApplet();
	else if(args[0]=='fork')
		this.forkExample();
	else if(args[0]=='open') {
		this.visualizeApplets(this.applets);
		this.setScreen("files")
	}
	else if(args[0]=='saveFS')
		this.exportApplet();
	else if(args[0]=='compile')
		this.packageApplet();
	else if(args[0]=='toggleMenu')
		this.sidebar.toggle();
	else if(args[0]=='toggleConsole') {
		let el = document.querySelector('#console');
		el.className = (el.className=='console_big') ? 'console_small' : 'console_big';
		document.getElementById('btn_toggleConsole').style.transform =
			(el.className=='console_big') ? 'scaleY(-1)' : 'scaleY(1)';
	}
	else if(args[0]=='toggleHelp')
		document.getElementById('apiref_panel').classList.toggle('visible');
	else if(args[0]=='createRes')
		this.createResource(args[1]);
	else if(args[0]=='closeAuxEditor')
		this.closeAuxEditor();
	else
		this.log('"'+args.join(' ')+'" not yet implemented');

	if(this.installEvt && (new Date())-this.tStart > 30000 && !this.installationRejected && args[0]!='run') {
		this.installEvt.prompt();
		this.installEvt.userChoice.then((choiceResult)=>{
			if (choiceResult.outcome !== 'accepted')
				this.installationRejected = true;
		});
		delete this.installEvt;
	}
};
return ide;
})(_app);

document.addEventListener('DOMContentLoaded', ()=>{ _app.init(); });
