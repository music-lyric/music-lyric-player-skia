import type { LayoutAlign, MainModule, Player, Renderer, RenderingConfig } from 'music-lyric-player-skia'

import { createMusicLyricPlayerModule } from 'music-lyric-player-skia'

import './style.css'

interface LyricSummary {
  version: number
  valid: boolean
  lineCount: number
}

const query = <T extends HTMLElement>(selector: string): T => {
  const element = document.querySelector<T>(selector)
  if (!element) {
    throw new Error(`missing element: ${selector}`)
  }
  return element
}

const fromHex = (text: string): Uint8Array => {
  const digits = text.replace(/[^0-9a-fA-F]/g, '')
  if (digits.length % 2 !== 0) {
    throw new Error('hex input has an odd number of digits')
  }

  const bytes = new Uint8Array(digits.length / 2)
  for (let i = 0; i < bytes.length; i += 1) {
    bytes[i] = Number.parseInt(digits.slice(i * 2, i * 2 + 2), 16)
  }
  return bytes
}

const formatTime = (seconds: number): string => {
  if (!Number.isFinite(seconds)) {
    return '0:00'
  }

  const total = Math.max(0, Math.floor(seconds))
  return `${Math.floor(total / 60)}:${String(total % 60).padStart(2, '0')}`
}

const canvas = query<HTMLCanvasElement>('#canvas')
const notice = query('#notice')
const telemetry = query('#telemetry')

const audio = query<HTMLAudioElement>('#audio')
const audioInput = query<HTMLInputElement>('#audio-file')
const fontInput = query<HTMLInputElement>('#font-file')
const lyricInput = query<HTMLInputElement>('#lyric-file')

const pasteButton = query<HTMLButtonElement>('#lyric-paste')
const pasteDialog = query<HTMLDialogElement>('#paste-dialog')
const pasteText = query<HTMLTextAreaElement>('#paste-text')

const toggleButton = query<HTMLButtonElement>('#toggle')
const seek = query<HTMLInputElement>('#seek')
const time = query('#time')

const sizeRange = query<HTMLInputElement>('#config-size')
const sizeValue = query('#config-size-value')
const gapRange = query<HTMLInputElement>('#config-gap')
const gapValue = query('#config-gap-value')
const alignSelect = query<HTMLSelectElement>('#config-align')
const patchArea = query<HTMLTextAreaElement>('#config-patch')
const applyButton = query<HTMLButtonElement>('#config-apply')

const report = (message: string, failed = false): void => {
  notice.textContent = message
  notice.classList.toggle('error', failed)
}

const messageOf = (error: unknown): string => (error instanceof Error ? error.message : String(error))

let module: MainModule
let player: Player
let renderer: Renderer

try {
  module = await createMusicLyricPlayerModule()

  player = new module.Player()

  const created = module.Renderer.create(player, '#canvas')
  if (!created) {
    throw new Error('the browser gave no WebGL 2 context for #canvas')
  }
  renderer = created
} catch (error) {
  telemetry.textContent = `failed: ${messageOf(error)}`
  throw error
}

// The renderer borrows the player, so it has to be released first.
window.addEventListener('beforeunload', () => {
  renderer.delete()
  player.delete()
})

let lineCount = 0
player.onLyricUpdate((summary: LyricSummary) => {
  lineCount = summary.lineCount
})

// The canvas backbuffer is sized in physical pixels by the module itself, so the page only has to report the CSS box and the ratio to scale it by.
const sync = (): void => {
  const dpr = window.devicePixelRatio || 1
  const rect = canvas.getBoundingClientRect()

  renderer.configure(Math.round(rect.width * dpr), Math.round(rect.height * dpr), dpr)
}

sync()
new ResizeObserver(sync).observe(canvas)

window.addEventListener('resize', sync)

const loadLyric = (bytes: Uint8Array): void => {
  try {
    player.updateLyric(bytes)
    report(`lyric loaded, ${lineCount} lines`)
  } catch (error) {
    report(`lyric rejected: ${messageOf(error)}`, true)
  }
}

// Skia has no system font stack inside the sandbox, so a family only resolves once its bytes have been handed over.
fontInput.addEventListener('change', async () => {
  const file = fontInput.files?.[0]
  if (!file) {
    return
  }

  const added = module.registerFont(new Uint8Array(await file.arrayBuffer()))
  report(added ? `font ${file.name} registered` : `font ${file.name} was already registered`)
})

lyricInput.addEventListener('change', async () => {
  const file = lyricInput.files?.[0]
  if (!file) {
    return
  }

  loadLyric(new Uint8Array(await file.arrayBuffer()))
})

pasteButton.addEventListener('click', () => {
  pasteText.value = ''
  pasteDialog.showModal()
})

pasteDialog.addEventListener('close', () => {
  if (pasteDialog.returnValue !== 'load') {
    return
  }

  try {
    loadLyric(fromHex(pasteText.value))
  } catch (error) {
    report(`lyric rejected: ${messageOf(error)}`, true)
  }
})

let audioUrl = ''

audioInput.addEventListener('change', () => {
  const file = audioInput.files?.[0]
  if (!file) {
    return
  }

  // Each pick leaks its blob URL unless the previous one is handed back first.
  if (audioUrl) {
    URL.revokeObjectURL(audioUrl)
  }
  audioUrl = URL.createObjectURL(file)

  audio.src = audioUrl
  report(`audio ${file.name} loaded`)
})

audio.addEventListener('loadedmetadata', () => {
  seek.max = String(audio.duration)
  seek.disabled = false
})

// The element owns the play head once it has a source, so the button only drives it and the player follows the events it emits.
audio.addEventListener('play', () => {
  player.playFrom(audio.currentTime * 1000)
  toggleButton.textContent = 'pause'
})

audio.addEventListener('pause', () => {
  player.pause()
  toggleButton.textContent = 'play'
})

// Pushing a time the player reads as a jump only moves the clock; re-anchoring the active line is what `playFrom` is for.
audio.addEventListener('seeked', () => {
  const now = audio.currentTime * 1000

  player.setExternalTime(now)
  player.playFrom(now)
  if (audio.paused) {
    player.pause()
  }
})

toggleButton.addEventListener('click', () => {
  if (audio.src) {
    if (audio.paused) {
      void audio.play()
    } else {
      audio.pause()
    }
    return
  }

  // Without an audio source the player stays on its own steady clock, so it is driven directly.
  if (player.currentPlaying) {
    player.pause()
  } else {
    player.play()
  }
  toggleButton.textContent = player.currentPlaying ? 'pause' : 'play'
})

seek.addEventListener('input', () => {
  audio.currentTime = Number(seek.value)
})

const applyConfig = (): void => {
  const patch: RenderingConfig = {
    layout: {
      align: alignSelect.value as LayoutAlign,
      gap: `${gapRange.value}px`,
    },
    line: {
      normal: {
        base: {
          font: { size: `${sizeRange.value}px` },
        },
      },
    },
  }

  try {
    renderer.updateConfig(JSON.stringify(patch))

    const raw = patchArea.value.trim()
    if (raw) {
      // Parsed here only so a typo reports as a JSON error rather than as a rejected patch.
      JSON.parse(raw)
      renderer.updateConfig(raw)
    }
    report('config applied')
  } catch (error) {
    report(`config rejected: ${messageOf(error)}`, true)
  }
}

const syncConfigLabels = (): void => {
  sizeValue.textContent = `${sizeRange.value}px`
  gapValue.textContent = `${gapRange.value}px`
}

sizeRange.addEventListener('input', syncConfigLabels)
gapRange.addEventListener('input', syncConfigLabels)
applyButton.addEventListener('click', applyConfig)

syncConfigLabels()
toggleButton.disabled = false

const frame = (): void => {
  // The element is the time source whenever it has one loaded, and the player has to see the new time before it ticks.
  if (audio.src) {
    player.setExternalTime(audio.currentTime * 1000)

    // Writing while the slider has focus would fight the drag that is moving it.
    if (document.activeElement !== seek) {
      seek.value = String(audio.currentTime)
    }
    time.textContent = `${formatTime(audio.currentTime)} / ${formatTime(audio.duration)}`
  }

  player.tick()
  renderer.renderFrame()

  telemetry.textContent = `v${module.version()} · ${canvas.width}×${canvas.height} · ${lineCount} lines · ${player.currentTime.toFixed(0)}ms · line ${player.currentActive}`
  requestAnimationFrame(frame)
}

requestAnimationFrame(frame)
