#include "playback/player.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

#include "line/content.h"
#include "utils/clock/steady.h"
#include "version.h"

namespace music_lyric_player::playback {
	namespace {
		/**
		 * Shared stateless real-time clock used when no clock is injected.
		 */
		const Clock& defaultClock() {
			static SteadyClock clock;
			return clock;
		}

		/**
		 * A parsed `major.minor.patch` triple; pre-release / build suffixes are ignored.
		 */
		struct SemVer {
			int major = 0;
			int minor = 0;
			int patch = 0;
		};

		/**
		 * Parses the leading numeric version triple; returns false when no major is present.
		 */
		bool parseSemVer(const std::string& text, SemVer& out) {
			int       major   = 0;
			int       minor   = 0;
			int       patch   = 0;
			const int matched = std::sscanf(text.c_str(), "%d.%d.%d", &major, &minor, &patch);
			if (matched < 1) {
				return false;
			}
			out = SemVer{major, minor, patch};
			return true;
		}

		/**
		 * Three-way comparison of two version triples: -1 / 0 / 1 for less / equal / greater.
		 */
		int compareSemVer(const SemVer& a, const SemVer& b) {
			if (a.major != b.major) {
				return a.major < b.major ? -1 : 1;
			}
			if (a.minor != b.minor) {
				return a.minor < b.minor ? -1 : 1;
			}
			if (a.patch != b.patch) {
				return a.patch < b.patch ? -1 : 1;
			}
			return 0;
		}

		/**
		 * Caret range check `^base`: at least the base and below the next breaking version.
		 * The breaking bump follows the base's left-most non-zero component, so `^1.2.3` allows
		 * up to `< 2.0.0`, `^0.2.3` up to `< 0.3.0` and `^0.0.3` up to `< 0.0.4`.
		 */
		bool satisfiesCaret(const std::string& version, const std::string& base) {
			SemVer v;
			SemVer b;
			if (!parseSemVer(version, v) || !parseSemVer(base, b)) {
				return false;
			}
			// Reject anything below the base version.
			if (compareSemVer(v, b) < 0) {
				return false;
			}
			// Exclusive upper bound: bump the base's left-most non-zero component.
			SemVer upper;
			if (b.major > 0) {
				upper = SemVer{b.major + 1, 0, 0};
			} else if (b.minor > 0) {
				upper = SemVer{0, b.minor + 1, 0};
			} else {
				upper = SemVer{0, 0, b.patch + 1};
			}
			return compareSemVer(v, upper) < 0;
		}
	} // namespace

	Player::Player()
	    : Player(defaultClock()) {}

	Player::Player(const Clock& clock)
	    : clockRef(clock) {
		this->configListenerId = this->config.onUpdate.add([this](const config::RootChange& changes, const config::Root&) {
			onConfigUpdate(changes);
		});
	}

	void Player::updateLyric(const ::lyric::Info& info) {
		// Reject parse results whose lyric format version is incompatible, clearing any current lyric.
		::lyric::Info target = info;
		if (!satisfiesCaret(info.version(), music_lyric_model::SCHEMA_VERSION)) {
			std::fprintf(stderr, "[music-lyric-player] ignored lyric with incompatible version \"%s\", expected \"^%s\"\n", info.version().c_str(), music_lyric_model::SCHEMA_VERSION);
			target = ::lyric::Info{};
		}

		pause();
		this->info = std::move(target);
		buildMergedLineEnd();

		this->offset.refreshFromMeta(this->info, this->config.current().offset.useMeta);
		if (this->config.current().offset.resetTempOnLyricChange) {
			this->offset.resetTemp();
		}

		this->activeIndex.clear();
		this->scanIndex = 0;
		this->seekMs    = 0.0;

		this->onLyricUpdate.emit(this->info);
		this->onLinesUpdate.emit(std::vector<int>{}, -1, false);
	}

	void Player::play(std::optional<double> seekMs) {
		pause();

		if (seekMs.has_value() && std::isfinite(*seekMs)) {
			this->seekMs = *seekMs;
			syncTime();
		}

		this->startMs = this->clockRef.now();
		this->playing = true;
		// Run one immediate pass so the active set is correct the instant playback starts.
		updateActiveLines(getEffectiveTime());

		this->onPlay.emit(getCurrentTime());
	}

	void Player::pause() {
		if (this->playing) {
			this->seekMs  = getCurrentTime();
			this->playing = false;
			this->onPause.emit(this->seekMs);
		}
	}

	void Player::tick() {
		if (!this->playing) {
			return;
		}
		updateActiveLines(getEffectiveTime());
	}

	void Player::dispose() {
		pause();
		this->onPlay.clear();
		this->onPause.clear();
		this->onLyricUpdate.clear();
		this->onLinesUpdate.clear();
		this->config.onUpdate.remove(this->configListenerId);

		this->activeIndex.clear();
		this->info = ::lyric::Info{};
	}

	void Player::updateTempOffset(double value) {
		this->offset.setTemp(value);
		syncTime();
	}

	std::vector<int> Player::matchLinesWithTime(double time) const {
		const double     effective = time + currentOffset();
		std::vector<int> index;
		for (int i = 0; i < this->info.lines_size(); ++i) {
			const ::lyric::Time* lineTime = music_lyric_model::getLineTime(this->info.lines(i));
			// Lines are sorted by start ascending, so the first line starting after `time` ends the scan.
			if ((lineTime ? static_cast<double>(lineTime->start()) : 0.0) > effective) {
				break;
			}
			if (this->merger.getMergedTime(i) > effective) {
				index.push_back(i);
			}
		}
		return bridgeActive(index);
	}

	double Player::convertContentTime(double contentTime) const {
		return contentTime - currentOffset();
	}

	bool Player::currentPlaying() const {
		return this->playing;
	}

	std::vector<int> Player::currentIndex() const {
		return bridgeActive(this->activeIndex);
	}

	int Player::currentActive() const {
		return getActiveIndex();
	}

	const ::lyric::Info& Player::currentInfo() const {
		return this->info;
	}

	double Player::currentTime() const {
		return getCurrentTime();
	}

	double Player::currentOffset() const {
		return this->offset.resolve(this->config.current().offset.global);
	}

	const Clock& Player::clock() const {
		return this->clockRef;
	}

	double Player::getCurrentTime() const {
		if (!this->playing) {
			return this->seekMs;
		}
		return this->seekMs + (this->clockRef.now() - this->startMs);
	}

	double Player::getEffectiveTime() const {
		return getCurrentTime() + currentOffset();
	}

	void Player::buildMergedLineEnd() {
		this->merger.build(this->info, this->config.current().mergeWindow, this->config.current().mergeLimit);
	}

	int Player::getActiveIndex() const {
		return this->activeIndex.empty() ? -1 : this->activeIndex.front();
	}

	std::vector<int> Player::bridgeActive(const std::vector<int>& index) const {
		if (!this->config.current().bridgeActive || index.size() < 2) {
			return index;
		}
		const int min = index.front();
		const int max = index.back();
		if (static_cast<int>(index.size()) == max - min + 1) {
			return index;
		}
		// Promote the sandwiched (already-ended) lines back so the range is contiguous.
		std::vector<int> bridged;
		for (int i = min; i <= max && i < this->info.lines_size(); ++i) {
			if (i >= 0) {
				bridged.push_back(i);
			}
		}
		return bridged;
	}

	void Player::emitLinesUpdate(bool isSeek) {
		this->onLinesUpdate.emit(bridgeActive(this->activeIndex), getActiveIndex(), isSeek);
	}

	void Player::syncTime(std::optional<double> time) {
		if (this->info.lines_size() == 0) {
			return;
		}

		const double effective = time.has_value() ? *time : getEffectiveTime();
		if (!std::isfinite(effective)) {
			return;
		}

		std::vector<int> index;
		int              firstIndex = this->info.lines_size();
		for (int i = 0; i < this->info.lines_size(); ++i) {
			const ::lyric::Time* lineTime = music_lyric_model::getLineTime(this->info.lines(i));
			if ((lineTime ? static_cast<double>(lineTime->start()) : 0.0) > effective) {
				firstIndex = i;
				break;
			}
			if (this->merger.getMergedTime(i) > effective) {
				index.push_back(i);
			}
		}

		this->scanIndex   = firstIndex;
		this->activeIndex = std::move(index);
		emitLinesUpdate(true);
	}

	void Player::updateActiveLines(double now) {
		bool             hasChanged = false;
		std::vector<int> newActive;

		for (const int infoIndex : this->activeIndex) {
			if (now >= this->merger.getMergedTime(infoIndex)) {
				hasChanged = true;
			} else {
				newActive.push_back(infoIndex);
			}
		}

		while (this->scanIndex < this->info.lines_size()) {
			const ::lyric::Time* lineTime = music_lyric_model::getLineTime(this->info.lines(this->scanIndex));
			if (now >= (lineTime ? static_cast<double>(lineTime->start()) : 0.0)) {
				if (now < this->merger.getMergedTime(this->scanIndex)) {
					newActive.push_back(this->scanIndex);
					hasChanged = true;
				}
				this->scanIndex++;
			} else {
				break;
			}
		}

		if (!hasChanged) {
			return;
		}

		this->activeIndex = std::move(newActive);
		emitLinesUpdate(false);
	}

	void Player::onConfigUpdate(const config::RootChange& changes) {
		// Toggling meta usage re-derives the lyric offset from the current info.
		const bool metaToggled = changes.offset.useMeta;
		if (metaToggled) {
			this->offset.refreshFromMeta(this->info, this->config.current().offset.useMeta);
		}
		// An offset change shifts effective time, so re-match active lines.
		if (metaToggled || changes.offset.global) {
			syncTime();
		}
		// Merge settings changed: rebuild merged ends and re-match.
		if (changes.mergeWindow || changes.mergeLimit) {
			buildMergedLineEnd();
			syncTime();
		}
	}
} // namespace music_lyric_player::playback
