# music-lyric-player-skia

A lyric player rendered with [Skia](https://skia.org/), compiled to WebAssembly.

This is the low level package: the raw [embind](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html) surface plus generated type definitions, and no hand written JavaScript. It ships exactly what the C++ exposes, so the driving loop, the resize observer and the font lookup are the page's job. For a higher level, DOM shaped API, use the facade package `@music-lyric-player/skia` instead.

## Install

```sh
npm install music-lyric-player-skia
```

The package is browser only. It needs WebGL2 and WebAssembly exception handling, both of which are available in current Chrome, Edge, Firefox and Safari. Node has no canvas to render into.

## Usage

```js
import createMusicLyricPlayerModule from 'music-lyric-player-skia';

const runtime = await createMusicLyricPlayerModule();

// Nothing is bundled, so register at least one font before any text can be drawn.
const font = await fetch('/fonts/body.ttf').then((response) => response.arrayBuffer());
runtime.registerFont(new Uint8Array(font));

const player = new runtime.Player();
const renderer = runtime.Renderer.create(player, '#lyric');
if (renderer === null) {
  throw new Error('the GPU stack failed to start');
}

// A lyric is the encoded bytes of a `parsed.Info` protobuf message.
const lyric = await fetch('/lyric.pb').then((response) => response.arrayBuffer());
player.updateLyric(new Uint8Array(lyric));

const canvas = document.querySelector('#lyric');
const audio = document.querySelector('audio');

const resize = () => {
  const ratio = window.devicePixelRatio;
  renderer.configure(Math.round(canvas.clientWidth * ratio), Math.round(canvas.clientHeight * ratio), ratio);
};
new ResizeObserver(resize).observe(canvas);
resize();

const frame = () => {
  player.setExternalTime(audio.currentTime * 1000);
  player.tick();
  renderer.renderFrame();
  requestAnimationFrame(frame);
};
requestAnimationFrame(frame);
```

Serve the `.wasm` next to the `.mjs`: the module resolves it from `import.meta.url`, so a bundler has to emit both as assets rather than inline the JavaScript alone.

## Fonts

The module carries no font data at all, and the platform fonts a browser exposes are not reachable from WebAssembly. Every family the config asks for has to be handed in as bytes first.

```js
runtime.registerFont(new Uint8Array(bytes)); // -> true when the set changed
```

Registration is process wide and additive, and every live renderer picks the new family up on its next frame. Registering the exact same file twice is a no op. When the config names no family, the first registered font is used, so a single call is enough to get text on screen.

## Configuration

The renderer takes a sparse JSON patch: whatever the object leaves out keeps its current value.

```js
renderer.updateConfig(JSON.stringify({ line: { normal: { base: { font: { size: '32px' } } } } }));
```

Malformed JSON throws. The shape of the patch is not part of the embind signature, so it is typed separately in `types/config.gen.d.ts`.

## Timing

A player starts on its own steady clock. The first `setExternalTime()` call hands the play head over to the page for good, after which a fresh time has to be pushed before every `tick()`. That is what makes an `<audio>` element, or any other source, the timing authority.

## Lifetime

The objects reaching JavaScript are embind handles over C++ memory, and the garbage collector does not free them. Release them explicitly, and release a renderer before the player it borrows:

```js
renderer.delete();
player.delete();
```

`dispose()` tears the internals down early, which is useful when the teardown order matters, and leaves the handle safe to `delete()` afterwards.

## Content Security Policy

WebAssembly compilation needs `'wasm-unsafe-eval'` in `script-src`. The embind glue also builds part of its type machinery with `new Function`, so a policy without `'unsafe-eval'` can reject the module outright.

## License

MIT
