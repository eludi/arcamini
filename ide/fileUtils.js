'use strict';
// Ported near-verbatim from https://github.com/eludi/elowo/blob/main/fileUtils.js
window.fileUtils = {
	saveText(data, filename, mime = 'text/plain') {
		const blob = new Blob([data], {type:mime});
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		a.download = filename;
		document.body.appendChild(a);
		a.click();
		a.remove();
		setTimeout(()=> URL.revokeObjectURL(url), 10000);
	},
	loadjs(urls, async, callback) {
		if(!Array.isArray(urls))
			urls = [ urls ];
		let numLoaded = 0;
		for(let i=0; i<urls.length; ++i) {
			let node=document.createElement('script');
			node.setAttribute("type","text/javascript");
			node.setAttribute("src", urls[i]);
			if(async)
				node.setAttribute('async','async');
			if(callback)
				node.addEventListener("load", ()=>{ if(++numLoaded==urls.length) callback(); }, false);
			document.getElementsByTagName("head")[0].appendChild(node);
		}
	},
	fileType(file) {
		if(typeof file == 'string')
			file = { name:file };
		if(file.type) {
			if(file.type.indexOf(';')>=0)
				return file.type.substring(0, file.type.indexOf(';'));
			return file.type;
		}
		let suffix = file.name.substr(file.name.lastIndexOf('.')+1).toLowerCase();
		switch(suffix) {
		case 'json':
			return 'application/json';
		case 'js':
			return 'application/javascript';
		case 'py':
			return 'text/x-python';
		case 'lua':
			return 'text/x-lua';
		case 'txt':
			return 'text/plain';
		case 'html':
			return 'text/html';
		case 'md':
			return 'text/markdown';
		case 'css':
			return 'text/css';
		case 'png':
			return 'image/png';
		case 'jpg': case 'jpeg':
			return 'image/jpeg';
		case 'svg':
			return 'image/svg+xml';
		case 'wav':
			return 'audio/wav';
		case 'mp3':
			return 'audio/mpeg';
		case 'ogg':
			return 'audio/ogg';
		case 'woff':
			return 'font/woff';
		case 'woff2':
			return 'font/woff2';
		case 'ttf':
			return 'font/ttf';
		}
		console.warn('unknown file type', suffix);
		return '';
	},
	suffix(mime) {
		switch(mime) {
		case 'application/javascript':
			return 'js';
		case 'text/x-python':
			return 'py';
		case 'text/x-lua':
			return 'lua';
		case 'text/plain':
			return 'txt';
		case 'text/markdown':
			return 'md';
		case 'image/svg+xml':
			return 'svg';
		default:
			return mime.substr(mime.indexOf('/')+1);
		}
	},
	isBinary(file) {
		let type = this.fileType(file);
		if(typeof type != 'string')
			return true;
		return (type.startsWith('text/') || type.endsWith('/json') || type.endsWith('javascript')) ?
			false : true;
	},
	baseName(name) {
		let suffixStart = name.lastIndexOf('.');
		return (suffixStart<0) ? name : name.substring(0, suffixStart);
	},
	// data: URL -> ArrayBuffer, for cache.put()-ing resources as real binary
	// Responses (Run) or zipping them as real files (export). Native
	// fetch() decodes base64/percent-encoding for us -- no vendored
	// base64 decoder needed, unlike elowo's packager.js.
	dataUrlToArrayBuffer(dataUrl) {
		return fetch(dataUrl).then((resp)=> resp.arrayBuffer());
	}
};
