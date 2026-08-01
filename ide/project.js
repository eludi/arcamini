'use strict';
// Base IDE namespace: the resources model (verbatim port of elowo's
// app.js resources Map, https://github.com/eludi/elowo/blob/main/app.js)
// plus minimal screen/log/error plumbing that ide.js builds on with
// Object.create(). Named project.js (not app.js) to avoid confusion with
// browser_runtime/app.js, which this file has nothing to do with -- the
// IDE never runs a game in-page, it only edits project data and hands it
// off to the unmodified browser_runtime via Cache Storage (see ide.js).
window._app = {
	resources: {
		data: new Map(),
		import: function(file, visualize) {
			let isBinary = fileUtils.isBinary(file);
			let ftype = fileUtils.fileType(file);
			if(isBinary && !ftype.startsWith("audio/") && !ftype.startsWith("image/")
				&& !ftype.startsWith("font/"))
			{
				return _app.error("invalid resource type "+file.type);
			}

			let reader = new FileReader();
			reader.addEventListener("load", ()=>{
				isBinary ? this.add(file.name, reader.result, visualize) :
					this.add(file.name, { resource:reader.result, mime:ftype }, visualize);
			}, false);
			if(isBinary)
				reader.readAsDataURL(file);
			else
				reader.readAsText(file);
		},
		add: function(name, data, visualize) {
			this.remove(name);
			let item = ((typeof data=='object')||!data.startsWith('data:')) ?
				data : this.instantiate(name, data);
			if(data.mime=='image/svg+xml' && (typeof item.url != 'string')) {
				item.url = item.resource;
				item.resource = (new DOMParser()).parseFromString(item.url, data.mime);
			}
			if(item.url && (!item.resource || (Object.keys(item.resource).length === 0 && item.resource.constructor === Object))) {
				let instance = this.instantiate(name, item.url);
				item.mime = instance.mime;
				item.resource = instance.resource;
			}
			if(item) {
				this.data.set(name, item);
				if(visualize)
					this.visualize(name, item);
			}
			return item;
		},
		instantiate(name, dataUrl) {
			let obj = null;
			if(name.toLowerCase().endsWith('woff2') && !dataUrl.startsWith('data:font/woff2'))
				dataUrl = 'data:font/woff2' + dataUrl.substr(dataUrl.indexOf(';'));
			else if(name.toLowerCase().endsWith('ttf') && !dataUrl.startsWith('data:font/ttf'))
				dataUrl = 'data:font/ttf' + dataUrl.substr(dataUrl.indexOf(';'));

			let mime = this.getMime(dataUrl);
			if(mime.startsWith("audio/"))
				obj = new Audio(dataUrl);
			else if(mime.startsWith("image/")) {
				obj = new Image();
				obj.src = dataUrl;
			}
			else if(mime.startsWith('font/')) {
				let fontName = name.substr(0, name.indexOf('.'));
				fontName = fontName.toLowerCase().split('-')
					.map((s) => s.charAt(0).toUpperCase() + s.substring(1)).join(' ');
				obj = new FontFace(fontName, 'url('+dataUrl+')');
				obj.load().then((loadedFace)=>{ document.fonts.add(loadedFace); }).catch(err=>{
					_app.error("loading font resource "+name+" failed: "+err);
				});
			}
			if(obj) {
				obj.title = name;
				obj.className = 'resource';
				return { resource:obj, url:dataUrl, mime:mime };
			}
			return null;
		},
		serialize: function() {
			let data = {};
			this.data.forEach((item, id)=>{
				let record = { mime:item.mime };
				if(item.url) {
					record.url = item.url;
					if(item.terms)
						record.terms = item.terms;
				}
				else for(let key in item)
					record[key] = item[key];
				data[id] = record;
			});
			return data;
		},
		visualize: function(name, item) {
			let ul = document.querySelector('#resources_list');
			let li = ul.appendChild(document.createElement('li'));
			let left = li.appendChild(document.createElement('div'));
			let center = li.appendChild(document.createElement('div'));
			center.className = 'center';
			let right = li.appendChild(document.createElement('div'));
			let h = center.appendChild(document.createElement('input'));
			li.dataset.name = h.value = name;
			h.addEventListener('change', (evt)=>{ this.rename(name, evt.target.value); });

			let details = center.appendChild(document.createElement('div'));
			let sz = Math.ceil(this.getSize(item.url ? item.url : item.resource)/1024);
			details.innerText = item.mime+', '+sz+'kB';

			if(item.mime.indexOf("image/")==0)
				left.appendChild(item.resource);
			else if(item.mime.indexOf("audio/")==0) {
				let icon = document.querySelector('#musical_note').cloneNode(true);
				icon.removeAttribute('id');
				icon.addEventListener('click', ()=>{ item.resource.play(); })
				left.appendChild(icon);
			}

			const createButtonFromTemplate = function(selector, value, onclick) {
				let template = document.querySelector(selector);
				let button = document.createElement('button');
				button.innerHTML = template.innerHTML;
				button.value = value;
				button.title = template.title;
				button.addEventListener('click', onclick);
				return button;
			}

			right.appendChild(createButtonFromTemplate('#btn_delete', name, (evt)=>{
				if(window.confirm('Delete '+name+'?'))
					this.remove(evt.currentTarget.value);
			}));
		},
		empty: function() {
			return this.data.size == 0;
		},
		reset: function(data, visualize) {
			let ul = document.querySelector('#resources_list');
			while (ul && ul.lastChild)
				ul.removeChild(ul.lastChild);

			this.data = new Map();
			if(data) for(let id in data)
				this.add(id, data[id], visualize);
		},
		rename: function(oldName, newName) {
			if(!this.data.has(oldName) || oldName == newName)
				return;
			if(this.data.has(newName))
				return _app.error("resource name " + newName + " is not unique");
			let item = this.data.get(oldName);
			this.remove(oldName);
			this.add(newName, item, true);
		},
		remove: function(name) {
			if(!this.data.has(name))
				return;
			this.data.delete(name);
			let ul = document.querySelector('#resources_list');
			for(let item=ul.firstChild; item!==null; item=item.nextSibling)
				if(item.dataset.name == name)
					return ul.removeChild(item);
		},
		create: function(mime) {
			let i=1, name='';
			do {
				name = 'resource'+i+'.'+fileUtils.suffix(mime);
				++i;
			} while(this.data.has(name));
			this.add(name, { resource:'', mime:mime }, true);
			return name;
		},
		get: function(name) {
			let item = this.data.get(name);
			if(!item)
				return null;
			return item.mime.endsWith('json') ? JSON.parse(item.resource) : item.resource;
		},
		getMime(dataUrl) {
			let mime = dataUrl.substring(5, dataUrl.indexOf(',', 5));
			let sepPos = mime.indexOf(';');
			return sepPos>0 ? mime.substr(0, sepPos) : mime;
		},
		getSize(dataUrl) {
			return dataUrl.length;
		}
	},
	currentApplet: 'myProject.js',
	currentMeta: null,

	setScreen(name) {
		let id = 'screen_'+name;
		if(!document.getElementById(id))
			return false;
		for(let elems = document.querySelectorAll('body > div'), i=0, el; el=elems[i]; ++i)
			el.style.display = (el.id!=id) ? 'none' :
				name.toLowerCase().endsWith('editor') ? 'flex' : 'block';
		return true;
	},
	error(msg) { this.log(msg,'Crimson'); this.setScreen('editor'); },
	log(msg, color) {
		let cons = document.querySelector('#console');
		if(color) {
			let div = cons.appendChild(document.createElement('div'));
			div.style.color = color;
			div.innerText = msg;
		}
		else
			cons.appendChild(document.createElement('div')).innerText = msg;
		cons.scrollTop = cons.scrollHeight;
	}
};
