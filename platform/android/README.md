# music-lyric-player-skia

A lyric player rendered with [Skia](https://skia.org/), packaged for Android.

The library draws into a `Surface` through Vulkan, falling back to GLES on devices without it, and ships the whole graphics stack inside one shared library. Nothing beyond the entry points below is visible to the app it links into, so a host already carrying its own copy of Skia cannot collide with this one.

## Install

```kotlin
dependencies {
    implementation("io.github.music-lyric.player:skia:0.3.0")
}
```

`minSdk 24`, arm64-v8a and x86_64. There is no runtime dependency: everything the library uses lives in `android.jar`.

## Usage

The quickest path is `LyricSurfaceView`, which is a `SurfaceView` with a player and a renderer already wired to it.

```kotlin
val view = LyricSurfaceView(context)

view.player.setListener(object : LyricPlayer.Listener {
    override fun onLinesUpdate(indices: IntArray, active: Int, isSeek: Boolean) {
        // On the main thread.
    }
})

view.renderer.post {
    view.player.updateLyric(lyricBytes)
    view.player.play()
}
```

`lyricBytes` are the encoded bytes of a `parsed.Info` protobuf message, which is what the lyric model produces.

Building the two by hand is the same thing without the view:

```kotlin
val player = LyricPlayer(MainThreadDispatch)
val renderer = LyricRenderer(player)

renderer.attach(surface, resources.displayMetrics.density)
```

## Threads

There is one render thread, and it belongs to the renderer.

- **Every call reaching native runs on it.** `LyricRenderer` posts its own; anything on `LyricPlayer` is the app's to post, which `renderer.post { }` is the way to do.
- **The player's state getters are the exception.** `currentPlaying`, `currentTime`, `currentOffset`, `currentActive` and `currentIndex` read a snapshot the render thread publishes once a frame, so any thread may read them. There is deliberately no synchronous getter: waiting on the render thread from a surface callback deadlocks.
- **Listener callbacks arrive wherever the dispatch puts them.** `LyricPlayer(MainThreadDispatch)` puts them on the main thread, and the default runs them on the render thread.

## Lifecycle

```
LyricPlayer()  ->  LyricRenderer(player)  ->  attach(surface)  ->  detach()  ->  renderer.close()  ->  player.close()
```

A renderer borrows the player it draws from, so release the renderer first. A renderer outlives the surfaces attached to it: a view whose surface is recreated calls `attach` and `detach` again rather than building another renderer.

`detach()` blocks until the render thread has let the surface go. That is the point of it: `surfaceDestroyed` may only return once nothing holds the surface any more.

## Fonts

`NativeLibrary.registerFont(bytes)` adds a font file to a process wide set that every renderer resolves families from, ahead of the system fonts. Registering is not per player, and every renderer alive takes the new set at once. Call it on the render thread.

## Config

`renderer.updateConfig(json)` merges a partial config over the current one. Keys the JSON leaves out keep their value, so it doubles as a sparse patch.

## License

MIT
