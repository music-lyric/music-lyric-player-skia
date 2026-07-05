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
		 * Caret check `^base`: same major and not below the base (the base major is >= 1 here).
		 */
		bool satisfiesCaret(const std::string& version, const std::string& base) {
			SemVer v;
			SemVer b;
			if (!parseSemVer(version, v) || !parseSemVer(base, b)) {
				return false;
			}
			if (v.major != b.major) {
				return false;
			}
			if (v.minor != b.minor) {
				return v.minor > b.minor;
			}
			return v.patch >= b.patch;
		}
	} // namespace

	Player::Player()
	    : Player(defaultClock()) {}

	Player::Player(const Clock& clock)
	    : clock_(clock) {
		configListenerId_ = config.onUpdate.add([this](const config::ChangeKeys& keys, const config::Root&) {
			onConfigUpdate(keys);
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
		info_ = std::move(target);
		buildMergedLineEnd();

		offset_.refreshFromMeta(info_, config.current().offset.useMeta);
		if (config.current().offset.resetTempOnLyricChange) {
			offset_.resetTemp();
		}

		activeIndex_.clear();
		scanIndex_ = 0;
		seekMs_    = 0.0;

		onLyricUpdate.emit(info_);
		onLinesUpdate.emit(std::vector<int>{}, -1, false);
	}

	void Player::play(std::optional<double> seekMs) {
		pause();

		if (seekMs.has_value() && std::isfinite(*seekMs)) {
			seekMs_ = *seekMs;
			syncTime();
		}

		startMs_ = clock_.nowMs();
		playing_ = true;
		// Run one immediate pass so the active set is correct the instant playback starts.
		updateActiveLines(getEffectiveTime());

		onPlay.emit(getCurrentTime());
	}

	void Player::pause() {
		if (playing_) {
			seekMs_  = getCurrentTime();
			playing_ = false;
			onPause.emit(seekMs_);
		}
	}

	void Player::tick() {
		if (!playing_) {
			return;
		}
		updateActiveLines(getEffectiveTime());
	}

	void Player::dispose() {
		pause();
		onPlay.clear();
		onPause.clear();
		onLyricUpdate.clear();
		onLinesUpdate.clear();
		config.onUpdate.remove(configListenerId_);

		activeIndex_.clear();
		info_ = ::lyric::Info{};
	}

	void Player::updateTempOffset(double value) {
		offset_.setTemp(value);
		syncTime();
	}

	std::vector<int> Player::matchLinesWithTime(double time) const {
		const double     effective = time + currentOffset();
		std::vector<int> index;
		for (int i = 0; i < info_.lines_size(); ++i) {
			const ::lyric::Time* lineTime = music_lyric_model::getLineTime(info_.lines(i));
			// Lines are sorted by start ascending, so the first line starting after `time` ends the scan.
			if ((lineTime ? static_cast<double>(lineTime->start()) : 0.0) > effective) {
				break;
			}
			if (merger_.getMergedTime(i) > effective) {
				index.push_back(i);
			}
		}
		return bridgeActive(index);
	}

	double Player::convertContentTime(double contentTime) const {
		return contentTime - currentOffset();
	}

	bool Player::currentPlaying() const {
		return playing_;
	}

	std::vector<int> Player::currentIndex() const {
		return bridgeActive(activeIndex_);
	}

	int Player::currentActive() const {
		return getActiveIndex();
	}

	const ::lyric::Info& Player::currentInfo() const {
		return info_;
	}

	double Player::currentTime() const {
		return getCurrentTime();
	}

	double Player::currentOffset() const {
		return offset_.resolve(config.current().offset.global);
	}

	const Clock& Player::clock() const {
		return clock_;
	}

	double Player::getCurrentTime() const {
		if (!playing_) {
			return seekMs_;
		}
		return seekMs_ + (clock_.nowMs() - startMs_);
	}

	double Player::getEffectiveTime() const {
		return getCurrentTime() + currentOffset();
	}

	void Player::buildMergedLineEnd() {
		merger_.build(info_, config.current().mergeWindow, config.current().mergeLimit);
	}

	int Player::getActiveIndex() const {
		return activeIndex_.empty() ? -1 : activeIndex_.front();
	}

	std::vector<int> Player::bridgeActive(const std::vector<int>& index) const {
		if (!config.current().bridgeActive || index.size() < 2) {
			return index;
		}
		const int min = index.front();
		const int max = index.back();
		if (static_cast<int>(index.size()) == max - min + 1) {
			return index;
		}
		// Promote the sandwiched (already-ended) lines back so the range is contiguous.
		std::vector<int> bridged;
		for (int i = min; i <= max && i < info_.lines_size(); ++i) {
			if (i >= 0) {
				bridged.push_back(i);
			}
		}
		return bridged;
	}

	void Player::emitLinesUpdate(bool isSeek) {
		onLinesUpdate.emit(bridgeActive(activeIndex_), getActiveIndex(), isSeek);
	}

	void Player::syncTime(std::optional<double> time) {
		if (info_.lines_size() == 0) {
			return;
		}

		const double effective = time.has_value() ? *time : getEffectiveTime();
		if (!std::isfinite(effective)) {
			return;
		}

		std::vector<int> index;
		int              firstIndex = info_.lines_size();
		for (int i = 0; i < info_.lines_size(); ++i) {
			const ::lyric::Time* lineTime = music_lyric_model::getLineTime(info_.lines(i));
			if ((lineTime ? static_cast<double>(lineTime->start()) : 0.0) > effective) {
				firstIndex = i;
				break;
			}
			if (merger_.getMergedTime(i) > effective) {
				index.push_back(i);
			}
		}

		scanIndex_   = firstIndex;
		activeIndex_ = std::move(index);
		emitLinesUpdate(true);
	}

	void Player::updateActiveLines(double now) {
		bool             hasChanged = false;
		std::vector<int> newActive;

		for (const int infoIndex : activeIndex_) {
			if (now >= merger_.getMergedTime(infoIndex)) {
				hasChanged = true;
			} else {
				newActive.push_back(infoIndex);
			}
		}

		while (scanIndex_ < info_.lines_size()) {
			const ::lyric::Time* lineTime = music_lyric_model::getLineTime(info_.lines(scanIndex_));
			if (now >= (lineTime ? static_cast<double>(lineTime->start()) : 0.0)) {
				if (now < merger_.getMergedTime(scanIndex_)) {
					newActive.push_back(scanIndex_);
					hasChanged = true;
				}
				scanIndex_++;
			} else {
				break;
			}
		}

		if (!hasChanged) {
			return;
		}

		activeIndex_ = std::move(newActive);
		emitLinesUpdate(false);
	}

	void Player::onConfigUpdate(const config::ChangeKeys& keys) {
		// Toggling meta usage re-derives the lyric offset from the current info.
		const bool metaToggled = config::keyHas(keys, config::Keys::offset::useMeta);
		if (metaToggled) {
			offset_.refreshFromMeta(info_, config.current().offset.useMeta);
		}
		// An offset change shifts effective time, so re-match active lines.
		if (metaToggled || config::keyHas(keys, config::Keys::offset::global)) {
			syncTime();
		}
		// Merge settings changed: rebuild merged ends and re-match.
		if (config::keyHas(keys, config::Keys::mergeWindow) || config::keyHas(keys, config::Keys::mergeLimit)) {
			buildMergedLineEnd();
			syncTime();
		}
	}
} // namespace music_lyric_player::playback
