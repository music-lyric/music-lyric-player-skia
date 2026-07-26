#include "audio.h"

#include <algorithm>
#include <cmath>
#include <string>

// This file drives Win32 through the wide-character APIs, so select the UNICODE resource macros before windows.h.
// NOMINMAX keeps windows.h from defining min/max as macros, which would break the std:: calls below.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <commdlg.h>

#include "miniaudio.h"

namespace example {
	namespace {
		// The play head is interpolated between device updates; cap the extrapolation so a stalled device cannot run the lyric away.
		constexpr double kMaxExtrapolationMs = 50.0;
	} // namespace

	/**
	 * Holds the miniaudio engine and the streamed track, keeping their types out of the public header.
	 */
	struct AudioPlayer::State {
		ma_engine     engine{};
		ma_sound      sound{};
		bool          engineReady = false;
		bool          soundReady  = false;
		double        sampleRate  = 0.0;
		float         volume      = 1.0f;
		std::uint64_t epoch       = 0;
	};

	AudioPlayer::AudioPlayer()
	    : state(std::make_unique<State>()) {}

	AudioPlayer::~AudioPlayer() {
		if (this->state->soundReady) {
			ma_sound_uninit(&this->state->sound);
		}
		if (this->state->engineReady) {
			ma_engine_uninit(&this->state->engine);
		}
	}

	bool AudioPlayer::load(const std::wstring& path) {
		if (path.empty()) {
			return false;
		}

		// The device is opened on first use, so a run that never loads audio touches no audio hardware.
		if (!this->state->engineReady) {
			if (ma_engine_init(nullptr, &this->state->engine) != MA_SUCCESS) {
				return false;
			}
			this->state->engineReady = true;
		}

		if (this->state->soundReady) {
			ma_sound_uninit(&this->state->sound);
			this->state->soundReady = false;
			this->state->sampleRate = 0.0;
		}

		// Stream from disk rather than decode up front: a full decode of a several-minute track would cost hundreds of megabytes.
		const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
		if (ma_sound_init_from_file_w(&this->state->engine, path.c_str(), flags, nullptr, nullptr, &this->state->sound) != MA_SUCCESS) {
			return false;
		}
		this->state->soundReady = true;

		// The cursor counts frames of the source, so its own rate converts them to milliseconds.
		ma_uint32 sampleRate = 0;
		if (ma_sound_get_data_format(&this->state->sound, nullptr, nullptr, &sampleRate, nullptr, 0) != MA_SUCCESS || sampleRate == 0) {
			sampleRate = ma_engine_get_sample_rate(&this->state->engine);
		}
		this->state->sampleRate = static_cast<double>(sampleRate);
		// The level is a player setting rather than a track one, so re-apply it to the freshly loaded sound.
		ma_sound_set_volume(&this->state->sound, this->state->volume);
		this->state->epoch += 1;
		return true;
	}

	bool AudioPlayer::loaded() const {
		return this->state->soundReady && this->state->sampleRate > 0.0;
	}

	bool AudioPlayer::playing() const {
		return this->state->soundReady && ma_sound_is_playing(&this->state->sound) == MA_TRUE;
	}

	bool AudioPlayer::finished() const {
		return this->state->soundReady && ma_sound_at_end(&this->state->sound) == MA_TRUE;
	}

	void AudioPlayer::play() {
		if (this->state->soundReady) {
			ma_sound_start(&this->state->sound);
		}
	}

	void AudioPlayer::pause() {
		if (this->state->soundReady) {
			ma_sound_stop(&this->state->sound);
		}
	}

	void AudioPlayer::seek(double positionMs) {
		if (!loaded()) {
			return;
		}

		const double length = duration();
		const double target = std::clamp(std::isfinite(positionMs) ? positionMs : 0.0, 0.0, length);
		ma_sound_seek_to_pcm_frame(&this->state->sound, static_cast<ma_uint64>(target * this->state->sampleRate / 1000.0));
		this->state->epoch += 1;
	}

	void AudioPlayer::setVolume(float value) {
		this->state->volume = std::clamp(std::isfinite(value) ? value : 1.0f, 0.0f, 1.0f);
		if (this->state->soundReady) {
			ma_sound_set_volume(&this->state->sound, this->state->volume);
		}
	}

	float AudioPlayer::volume() const {
		return this->state->volume;
	}

	double AudioPlayer::position() const {
		if (!loaded()) {
			return 0.0;
		}

		ma_uint64 frames = 0;
		if (ma_sound_get_cursor_in_pcm_frames(&this->state->sound, &frames) != MA_SUCCESS) {
			return 0.0;
		}
		return static_cast<double>(frames) * 1000.0 / this->state->sampleRate;
	}

	double AudioPlayer::duration() const {
		if (!loaded()) {
			return 0.0;
		}

		ma_uint64 frames = 0;
		if (ma_sound_get_length_in_pcm_frames(&this->state->sound, &frames) != MA_SUCCESS) {
			return 0.0;
		}
		return static_cast<double>(frames) * 1000.0 / this->state->sampleRate;
	}

	std::uint64_t AudioPlayer::epoch() const {
		return this->state->epoch;
	}

	PlaybackClock::PlaybackClock(const AudioPlayer& source)
	    : source(source) {}

	double PlaybackClock::now() const {
		// No track yet: keep reporting wall time so the demo behaves as it did before audio existed.
		if (!this->source.loaded()) {
			this->primed = false;
			return this->wall.now();
		}

		const double        wallNow      = this->wall.now();
		const double        audioNow     = this->source.position();
		const std::uint64_t currentEpoch = this->source.epoch();

		// A load or a seek moves the play head discontinuously, so re-anchor and let the reported time jump with it.
		if (!this->primed || currentEpoch != this->epoch) {
			this->epoch       = currentEpoch;
			this->anchorAudio = audioNow;
			this->anchorWall  = wallNow;
			this->lastValue   = audioNow;
			this->primed      = true;
			return audioNow;
		}

		// A stopped track holds its position, so report it verbatim and keep the anchor fresh for the next resume.
		if (!this->source.playing()) {
			this->anchorAudio = audioNow;
			this->anchorWall  = wallNow;
			this->lastValue   = audioNow;
			return audioNow;
		}

		if (audioNow != this->anchorAudio) {
			this->anchorAudio = audioNow;
			this->anchorWall  = wallNow;
		}

		// Interpolate on wall time between the device's coarse updates, without drifting away when it stalls or stepping backwards.
		double value = std::min(this->anchorAudio + (wallNow - this->anchorWall), this->anchorAudio + kMaxExtrapolationMs);
		value        = std::max(value, this->lastValue);
		this->lastValue = value;
		return value;
	}

	std::optional<std::wstring> promptAudioFile(void* ownerHwnd) {
		wchar_t buffer[MAX_PATH] = {};

		// miniaudio decodes wav, flac and mp3 out of the box; anything else is left to the all-files entry.
		OPENFILENAMEW dialog = {};
		dialog.lStructSize   = sizeof(dialog);
		dialog.hwndOwner     = static_cast<HWND>(ownerHwnd);
		dialog.lpstrFilter   = L"Audio files\0*.mp3;*.flac;*.wav\0All files\0*.*\0";
		dialog.lpstrFile     = buffer;
		dialog.nMaxFile      = MAX_PATH;
		dialog.lpstrTitle    = L"Open an audio file";
		dialog.Flags         = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

		if (GetOpenFileNameW(&dialog) == FALSE) {
			return std::nullopt;
		}
		return std::wstring(buffer);
	}
} // namespace example
