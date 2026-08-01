'use strict';
// Export: zip the current project as a standalone game directory shaped
// exactly like minigames/ -- manifest.json + entry script + resource
// files -- ready to drop next to any browser_runtime/ checkout and open
// via ?game=<dir>/. Adapted from elowo's packager.js
// (https://github.com/eludi/elowo/blob/main/packager.js), but the
// templates it downloaded/zipped are gone: there's nothing arcamini-shaped
// to fetch, the project's own manifest.json/script/resources *are* the
// whole output. No base64 decoding needed either -- JSZip accepts a
// Promise<ArrayBuffer> directly, and fetch() already decodes data: URLs.
fileUtils.loadjs('lib/jszip.min.js', false, ()=>{
	_app.packageApplet = async function() {
		this.storeCurrentApplet();
		const zip = new JSZip();
		zip.file('manifest.json', JSON.stringify(this.buildManifest(), null, '\t'));
		zip.file(this.entryFilename, this.editor.getContent());

		const resources = this.resources.serialize();
		for(let name in resources) {
			const item = resources[name];
			if(item.url)
				zip.file(name, fileUtils.dataUrlToArrayBuffer(item.url));
			else if(typeof item.resource === 'string')
				zip.file(name, item.resource);
		}

		const blob = await zip.generateAsync({type:'blob', compression:'DEFLATE'});
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		a.download = fileUtils.baseName(this.currentApplet) + '.zip';
		document.body.appendChild(a);
		a.click();
		a.remove();
		setTimeout(()=> URL.revokeObjectURL(url), 10000);
		this.log('exported ' + a.download);
	};
});
