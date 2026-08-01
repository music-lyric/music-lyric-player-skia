#include <exception>
#include <filesystem>
#include <string>

#include "audio.h"
#include "backend/font/font.h"
#include "backend/gpu/vulkan/vulkan.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkRect.h"
#include "lyric_input.h"
#include "music_lyric_model.h"
#include "panel.h"
#include "playback/player.h"
#include "rendering/config/config.h"
#include "rendering/renderer.h"
#include "state.h"
#include "text.h"
#include "utils/clock/steady.h"
#include "utils/logger/logger.h"
#include "window.h"

#ifndef MUSIC_LYRIC_PLAYER_VERSION
#define MUSIC_LYRIC_PLAYER_VERSION "dev"
#endif

namespace {
	constexpr music_lyric_player::utils::Logger logger{"ExampleApp"};

	// How fast the frame-timing readout eases toward its latest sample: low enough that the digits settle, high enough that a stall still shows up.
	constexpr double kTimingSmoothing = 0.1;

	/**
	 * Returns the file name shown for an audio path.
	 */
	std::string audioLabel(const std::wstring& path) {
		return example::wideToUtf8(std::filesystem::path(path).filename().wstring());
	}

	/**
	 * Returns the text of the line the player currently highlights, or empty when there is none.
	 */
	std::string activeLineText(const music_lyric_player::playback::Player& player) {
		const int index = player.currentActive();
		if (index < 0) {
			return {};
		}

		const auto& lines = player.currentInfo().lines;
		if (static_cast<std::size_t>(index) >= lines.size()) {
			return {};
		}
		return music_lyric_model::parsed::getParsedLineText(lines[static_cast<std::size_t>(index)]);
	}
} // namespace

int main() {
	// Every log line below carries UTF-8, which the console mangles until it is told what it is being handed.
	example::useUtf8Console();

	example::Window window;
	if (!window.init(1440, 900, "Music Lyric Player")) {
		logger.error("failed to initialise the window");
		return 1;
	}

	auto surface = music_lyric_player::backend::gpu::vulkan::createSurface({window.hwnd()});
	if (surface == nullptr) {
		logger.error("failed to create the backend surface");
		return 1;
	}

	// Naming the build and what it draws with up front, since that is what a report about this demo has to identify before anything else.
	logger.info("version %s", MUSIC_LYRIC_PLAYER_VERSION);
	logger.info("drawing through Vulkan at %dx%d physical pixels, device pixel ratio %.2f", surface->width(), surface->height(), window.devicePixelRatio());

	sk_sp<SkFontMgr> fontManager = music_lyric_player::backend::font::fontManager();

	// The player follows the audio play head, while the renderer keeps wall time: its transitions must keep easing while playback is paused.
	example::AudioPlayer                    audio;
	example::PlaybackClock                  playbackClock(audio);
	music_lyric_player::utils::SteadyClock  renderClock;
	music_lyric_player::playback::Player    player(playbackClock);
	music_lyric_player::rendering::Renderer renderer(player, fontManager, renderClock);

	// DirectWrite cannot resolve a generic "sans-serif" family, so pick a concrete system family covering CJK and latin.
	// Colours follow the web playground: a white lyric area, left-aligned, with the web default line styling.
	// Re-applied whenever the settings editor drops every override, so resetting lands on the demo's look rather than the library's dark defaults.
	const auto applyDefaults = [&renderer]() {
		renderer.config.modify([](music_lyric_player::rendering::config::Root& cfg) {
			cfg.layout.align                 = music_lyric_player::rendering::config::layout::Align::Left;
			cfg.layout.gap                   = "45px";
			cfg.container.backgroundColor    = "#ffffff";
			cfg.line.normal.base.font.size   = "50px";
			cfg.line.normal.base.font.family = "MiSans";
			// The web default is black text dimmed by state; skia otherwise defaults these to white for a dark frame.
			cfg.line.normal.base.style.normal.color                       = "#000000";
			cfg.line.normal.base.style.normal.opacity                     = 0.6;
			cfg.line.normal.base.style.active.color                       = "#000000";
			cfg.line.normal.base.style.active.opacity                     = 1.0;
			cfg.line.normal.base.style.played.color                       = "#000000";
			cfg.line.normal.base.style.played.opacity                     = 0.4;
			cfg.line.interlude.style.normal.color                         = "#000000";
			cfg.line.interlude.style.normal.opacity                       = 0.2;
			cfg.line.interlude.style.active.color                         = "#000000";
			cfg.line.interlude.style.active.opacity                       = 0.8;
			cfg.scroll.anchor                                             = 0.35;
			cfg.scroll.animation.mode                                     = music_lyric_player::rendering::config::scroll::Mode::Stagger;
			cfg.scroll.animation.stagger.duration                         = 500;
			cfg.scroll.animation.stagger.easing                           = "ease";
			cfg.scroll.animation.stagger.step                             = 40;
			cfg.scroll.animation.stagger.range                            = 4;
			cfg.line.normal.main.syllable.word.animation.mask.enabled     = true;
			cfg.line.normal.main.syllable.word.animation.floating.enabled = true;
		});
	};
	applyDefaults();

	example::ControlPanel panel;
	if (!panel.init(window.handle(), window.devicePixelRatio())) {
		logger.warn("the control panel is unavailable, which leaves the demo with no controls at all");
	}

	example::AppState state = example::loadState();
	audio.setVolume(state.audio.volume);
	// The editor's overrides land on top of the demo defaults, so a look tweaked in the panel survives a restart.
	renderer.config.merge(state.settings);

	bool        paused = false;
	std::string trackLabel;
	std::string lyricLabel;

	// Mirrors the current session to disk; called after each meaningful change so a forced exit cannot lose it.
	const auto persist = [&]() {
		state.audio.volume = audio.volume();
		example::saveState(state);
	};

	// Moves the track and the lyric timeline together, preserving the paused state.
	const auto seekTo = [&](double positionMs) {
		double       target = positionMs > 0.0 ? positionMs : 0.0;
		const double length = audio.duration();
		if (length > 0.0 && target > length) {
			target = length;
		}

		audio.seek(target);
		player.play(target);
		if (paused) {
			player.pause();
		} else {
			audio.play();
		}
	};

	// Loads a parsed lyric, restarts both timelines from the top and records where it came from for the sidebar.
	const auto loadLyric = [&](const music_lyric_model::parsed::Info& info, const std::string& source) {
		paused = false;
		player.updateLyric(info);
		lyricLabel = source;
		seekTo(0.0);
	};

	const auto togglePause = [&]() {
		paused = !paused;
		if (paused) {
			// Stop the device first so the player samples an already-frozen play head.
			audio.pause();
			player.pause();
		} else if (audio.loaded() && audio.finished()) {
			// Resuming a track that ran to its end restarts it instead of sitting stuck at the last frame.
			seekTo(0.0);
		} else {
			// Anchor the player at the paused play head before the device starts advancing it.
			player.play();
			audio.play();
		}
	};

	const auto openAudio = [&]() {
		const auto picked = example::promptAudioFile(window.hwnd());
		if (!picked) {
			return;
		}
		if (!audio.load(*picked)) {
			example::reportError(window.hwnd(), "Failed to open the audio file.");
			return;
		}

		state.audio.path = example::wideToUtf8(*picked);
		trackLabel       = audioLabel(*picked);
		paused           = false;
		seekTo(0.0);
		persist();
	};

	const auto loadLyricHex = [&]() {
		const auto entered = example::promptHexLyric(window.hwnd(), state.lyric.hex);
		if (!entered) {
			return;
		}

		const auto bytes = example::decodeHex(*entered);
		if (!bytes) {
			example::reportError(window.hwnd(), "The input is not valid hex.");
			return;
		}

		try {
			const music_lyric_model::parsed::Info info = music_lyric_model::parsed::decodeParsedInfo(*bytes);
			state.lyric.hex                            = *entered;
			loadLyric(info, "loaded");
			persist();
		} catch (const std::exception& error) {
			example::reportError(window.hwnd(), std::string("Failed to decode the lyric: ") + error.what());
		}
	};

	// Restore the audio picked on a previous run first, so a restored lyric starts against it.
	if (!state.audio.path.empty()) {
		const std::wstring path = example::utf8ToWide(state.audio.path);
		if (audio.load(path)) {
			trackLabel = audioLabel(path);
			logger.info("restored the track \"%s\"", trackLabel.c_str());
		} else {
			logger.warn("the track kept from the previous run no longer opens: %s", state.audio.path.c_str());
			state.audio.path.clear();
		}
	}

	// Restore the lyric loaded on a previous run, so externally supplied input persists across launches.
	if (!state.lyric.hex.empty()) {
		if (const auto bytes = example::decodeHex(state.lyric.hex)) {
			try {
				loadLyric(music_lyric_model::parsed::decodeParsedInfo(*bytes), "restored");
				logger.info("restored a lyric of %zu lines", player.currentInfo().lines.size());
			} catch (const std::exception& error) {
				logger.error("failed to decode the persisted lyric: %s", error.what());
				state.lyric.hex.clear();
			}
		}
	}

	// Frame timing for the bar's readout: the loop's rate, plus the three spans the host can time from outside the surface.
	// Raw numbers change far too fast to read, so what the panel shows is eased toward each new sample.
	example::FrameTiming sample;
	example::FrameTiming timing;
	double               frameStartMs = renderClock.now();

	// Eases one displayed number toward its latest sample, starting from the sample itself so the first frame is not a ramp up from zero.
	const auto ease = [](double& shown, double value) { shown = shown > 0.0 ? shown + (value - shown) * kTimingSmoothing : value; };

	while (!window.shouldClose()) {
		window.pollEvents();

		if (window.pollResized()) {
			int frameWidth  = 0;
			int frameHeight = 0;
			window.framebufferSize(frameWidth, frameHeight);
			surface->handleResize(frameWidth, frameHeight);
		}
		player.tick();

		// A track stopped by anything other than a pause (most often reaching its end) counts as paused, so the controls and status stay truthful.
		if (audio.loaded() && !audio.playing() && !paused) {
			paused = true;
		}

		example::PanelState view;
		view.hasAudio = audio.loaded();
		view.hasLyric = !player.currentInfo().lines.empty();
		// Playing means the timeline is actually advancing: a loaded track follows the device, otherwise the wall-clock lyric follows the pause flag.
		view.playing    = !paused && (view.hasAudio || view.hasLyric);
		view.positionMs = player.currentTime();
		view.durationMs = audio.duration();
		view.volume     = audio.volume();
		view.activeLine = player.currentActive();
		view.lineCount  = static_cast<int>(player.currentInfo().lines.size());
		view.trackName  = trackLabel;
		view.lyricName  = lyricLabel;
		view.activeText = activeLineText(player);
		view.config     = &renderer.config.current();
		view.overrides  = &state.settings;

		// Raw per-frame numbers change far too fast to read, so both are eased toward the latest sample.
		const double frameEndMs = renderClock.now();
		sample.fps              = frameEndMs > frameStartMs ? 1000.0 / (frameEndMs - frameStartMs) : 0.0;
		frameStartMs            = frameEndMs;

		ease(timing.fps, sample.fps);
		ease(timing.totalMs, sample.totalMs);
		ease(timing.acquireMs, sample.acquireMs);
		ease(timing.drawMs, sample.drawMs);
		ease(timing.presentMs, sample.presentMs);
		view.timing = timing;

		const double acquireStartMs = renderClock.now();
		double       drawStartMs    = 0.0;
		double       drawEndMs      = 0.0;

		example::PanelActions actions;
		surface->renderFrame([&](SkCanvas* canvas) {
			drawStartMs = renderClock.now();

			const int panelWidth  = panel.width();
			const int frameWidth  = surface->width();
			const int frameHeight = surface->height();
			const int lyricWidth  = frameWidth - panelWidth;
			// The transport bar runs along the bottom of whatever the sidebar leaves, so the lyrics stop above it.
			const int lyricHeight = frameHeight - panel.controlsHeight();

			// The renderer always paints from its own origin, so the lyric area is clipped and shifted past the panel.
			if (lyricWidth > 0 && lyricHeight > 0) {
				canvas->save();
				canvas->clipRect(SkRect::MakeXYWH(static_cast<float>(panelWidth), 0.0f, static_cast<float>(lyricWidth), static_cast<float>(lyricHeight)));
				canvas->translate(static_cast<float>(panelWidth), 0.0f);
				renderer.setViewport(lyricWidth, lyricHeight, window.devicePixelRatio());
				renderer.render(canvas);
				canvas->restore();
			}

			actions = panel.render(canvas, view, frameWidth, frameHeight);

			drawEndMs = renderClock.now();
		});

		// The callback is skipped whenever the surface cannot present, so a frame that drew nothing books its whole span as waiting for a backbuffer.
		const double surfaceEndMs = renderClock.now();
		const bool   drew         = drawEndMs > 0.0;
		sample.totalMs            = surfaceEndMs - acquireStartMs;
		sample.acquireMs          = (drew ? drawStartMs : surfaceEndMs) - acquireStartMs;
		sample.drawMs             = drew ? drawEndMs - drawStartMs : 0.0;
		sample.presentMs          = drew ? surfaceEndMs - drawEndMs : 0.0;

		// Applied once the frame is done, so a seek or a modal file dialog never runs mid-paint.
		if (actions.settingsReset) {
			// Dropping the store alone would leave the renderer holding the old values, so the whole chain is rebuilt from the defaults.
			state.settings = {};
			renderer.config.reset();
			applyDefaults();
			persist();
		}
		if (actions.settingsChanged) {
			// The store already carries every override, and merging only folds in its assigned leaves, so re-merging is idempotent.
			renderer.config.merge(state.settings);
		}
		if (actions.settingsCommitted) {
			persist();
		}
		if (actions.volume) {
			audio.setVolume(*actions.volume);
		}
		if (actions.volumeCommitted) {
			// The drag has ended, so store the level that was applied live while it moved.
			persist();
		}
		if (actions.seek) {
			seekTo(*actions.seek);
		}
		if (actions.restart) {
			paused = false;
			seekTo(0.0);
		}
		if (actions.togglePause) {
			togglePause();
		}
		if (actions.openAudio) {
			openAudio();
		}
		if (actions.loadLyric) {
			loadLyricHex();
		}
	}

	// A clean exit still writes once, catching anything not already flushed by a discrete change above.
	persist();
	return 0;
}
