import { defineConfig } from 'vite'

export default defineConfig({
	optimizeDeps: {
		// The emscripten glue finds its wasm through `new URL('music-lyric-player.wasm', import.meta.url)`, which only resolves while the module is served from the directory that holds it.
		// Pre-bundling would rewrite the module into `.vite/deps`, where the wasm is not, and the load would 404.
		exclude: ['music-lyric-player-skia']
	},
	server: {
		// Windows commonly reserves the range Vite's default 5173 falls in, and a reserved port fails to bind rather than reporting itself as taken, so Vite cannot step over it on its own.
		port: 9101,
		fs: {
			// The package arrives as a `file:` dependency, so it is a symlink whose real path lies outside this directory.
			allow: ['.', '../../platform/web']
		}
	}
})
