// Root-scoped service worker for arcamini-ide (see ide/index.html). Lives at
// the repository root (not inside ide/) so its scope covers both ide/ and
// browser_runtime/ -- on static hosting there's no way to widen SW scope
// via headers, only by moving the script itself up the directory tree.
//
// Generic cache-first fetch handler, ported from
// https://github.com/eludi/elowo's serviceworker.js. No custom routing
// logic: ide.js writes edited project files into the 'ide-run' cache under
// the exact URLs browser_runtime/index.html will request them at (see
// ide/ide.js's run()), and this handler transparently serves them instead
// of hitting the network -- browser_runtime itself needs no changes at all.
//
// Deliberately does NOT precache browser_runtime's static files: only what
// ide.js explicitly cache.put()s (into 'ide-run') is ever served from
// cache, so a plain, non-IDE visit to browser_runtime/ is unaffected even
// while this worker is active and controlling the origin.
const version = '1';
const cacheWhitelist = ['ide-run', 'ide-shell-' + version];

this.addEventListener('install', (event) => {
	this.skipWaiting();
});

this.addEventListener('activate', (event) => {
	event.waitUntil(
		this.clients.claim().then(() => caches.keys()).then((cacheNames) => {
			return Promise.all(cacheNames.map((name) => {
				if (cacheWhitelist.indexOf(name) === -1)
					return caches.delete(name);
			}));
		})
	);
});

this.addEventListener('fetch', (event) => {
	event.respondWith(
		// ignoreSearch: browser_runtime's JS driver appends a `?_t=N`
		// cache-busting query string to force fresh ES module evaluation on
		// every scene load (see app.js's jsDriver.load) -- match on path only
		// so a cache-seeded entry still hits regardless of that suffix.
		caches.match(event.request, {ignoreSearch: true}).then((response) => {
			if (event.request.cache === 'only-if-cached' && event.request.mode !== 'same-origin')
				return;
			return response || fetch(event.request);
		})
	);
});
