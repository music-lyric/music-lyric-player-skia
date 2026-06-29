#include "base/config.h"

namespace music_lyric_player::base {
	void ConfigManager::update(const ConfigPatch& patch) {
		ChangeKeys changed;

		if (patch.bridgeActive && *patch.bridgeActive != current_.bridgeActive) {
			current_.bridgeActive = *patch.bridgeActive;
			changed.insert("bridgeActive");
		}
		if (patch.mergeWindow && *patch.mergeWindow != current_.mergeWindow) {
			current_.mergeWindow = *patch.mergeWindow;
			changed.insert("mergeWindow");
		}
		if (patch.mergeLimit && *patch.mergeLimit != current_.mergeLimit) {
			current_.mergeLimit = *patch.mergeLimit;
			changed.insert("mergeLimit");
		}
		if (patch.offset.global && *patch.offset.global != current_.offset.global) {
			current_.offset.global = *patch.offset.global;
			changed.insert("offset.global");
		}
		if (patch.offset.useMeta && *patch.offset.useMeta != current_.offset.useMeta) {
			current_.offset.useMeta = *patch.offset.useMeta;
			changed.insert("offset.useMeta");
		}
		if (patch.offset.resetTempOnLyricChange && *patch.offset.resetTempOnLyricChange != current_.offset.resetTempOnLyricChange) {
			current_.offset.resetTempOnLyricChange = *patch.offset.resetTempOnLyricChange;
			changed.insert("offset.resetTempOnLyricChange");
		}

		if (!changed.empty()) {
			onUpdate.emit(changed, current_);
		}
	}

	void ConfigManager::reset() {
		current_ = Config{};
		onReset.emit(current_);
	}
} // namespace music_lyric_player::base
