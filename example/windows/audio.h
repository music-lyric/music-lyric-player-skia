#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_AUDIO_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_AUDIO_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "utils/clock/clock.h"
#include "utils/clock/steady.h"

namespace example {
	/**
	 * A single-track audio player backed by miniaudio, streaming from disk so a long track costs little memory.
	 * The miniaudio types stay behind a pimpl so its multi-megabyte header never reaches another translation unit.
	 */
	class AudioPlayer {
	public:
		AudioPlayer();
		~AudioPlayer();

		AudioPlayer(const AudioPlayer&)            = delete;
		AudioPlayer& operator=(const AudioPlayer&) = delete;

		/**
		 * Opens `path` as the current track, replacing any previous one and leaving it stopped at the start.
		 * Returns false when the audio device or the file cannot be opened.
		 */
		bool load(const std::wstring& path);

		/**
		 * Reports whether a track is loaded and ready to play.
		 */
		bool loaded() const;

		/**
		 * Reports whether the play head is currently advancing.
		 */
		bool playing() const;

		/**
		 * Reports whether the current track has played through to its end.
		 */
		bool finished() const;

		/**
		 * Starts or resumes the current track.
		 */
		void play();

		/**
		 * Stops the current track, leaving the play head where it is.
		 */
		void pause();

		/**
		 * Moves the play head to `positionMs`, clamped to the track length.
		 */
		void seek(double positionMs);

		/**
		 * Sets the output level, where 1 is the track's own loudness; the value survives a later load.
		 */
		void setVolume(float value);

		/**
		 * Returns the current output level.
		 */
		float volume() const;

		/**
		 * Returns the play head in milliseconds, already reflecting a seek the mixing thread has yet to apply.
		 */
		double position() const;

		/**
		 * Returns the track length in milliseconds, or zero when no track is loaded.
		 */
		double duration() const;

		/**
		 * Returns a counter bumped by every load and seek, letting a clock detect a jump in the play head.
		 */
		std::uint64_t epoch() const;

	private:
		struct State;

		std::unique_ptr<State> state;
	};

	/**
	 * A `Clock` reporting the audio play head, so the lyric timeline follows the track instead of wall time.
	 * The device only publishes its position once per mixing period, so the value is interpolated on wall time in between; without that, the buffer-sized steps would reach the per-word animations.
	 * With no track loaded it falls back to wall time, preserving the demo's behaviour before any audio is opened.
	 */
	class PlaybackClock : public music_lyric_player::utils::Clock {
	public:
		/**
		 * Binds the clock to `source`, which must outlive it.
		 */
		explicit PlaybackClock(const AudioPlayer& source);

		/**
		 * Returns the interpolated play head in milliseconds.
		 */
		double now() const override;

	private:
		const AudioPlayer&                     source;
		music_lyric_player::utils::SteadyClock wall;

		mutable std::uint64_t epoch       = 0;
		mutable double        anchorAudio = 0.0;
		mutable double        anchorWall  = 0.0;
		mutable double        lastValue   = 0.0;
		mutable bool          primed      = false;
	};

	/**
	 * Shows the system file picker for an audio file, owned by the demo window.
	 * Returns the selected path, or nullopt when the user cancels.
	 */
	std::optional<std::wstring> promptAudioFile(void* ownerHwnd);
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_AUDIO_H_
