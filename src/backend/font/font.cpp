#include "backend/font/font.h"

#include <mutex>
#include <utility>
#include <vector>

#include "backend/font/composite.h"
#include "backend/font/platform.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkSpan.h"

namespace music_lyric_player::backend::font {
	namespace {
		/**
		 * The registered files and the managers standing over them, shared by everything in the process.
		 * A layer is rebuilt only once what it was built from has changed, which is what leaves a renderer asking for its manager costing one reference.
		 * The lock covers this state alone: a manager is immutable once handed out, so the lookups running off it never reach here.
		 */
		struct State {
			std::mutex mutex;

			std::vector<sk_sp<SkData>> files;
			sk_sp<SkFontMgr>           registered; // stands over `files`, and is null while there are none or the layer went stale
			sk_sp<SkFontMgr>           system;     // opened once, and null on a platform carrying no font stack
			bool                       systemOpened = false;
			sk_sp<SkFontMgr>           composed; // what `fontManager()` hands out, and null while stale
		};

		/**
		 * Returns the process-wide font state, which is deliberately never torn down.
		 * The layers are meant to outlive every renderer in any case, since an Android activity is recreated on each rotation and reopening the platform stack walks every installed family again.
		 * Releasing them from a static destructor would therefore free nothing sooner, and would only move the teardown to the worst moment on offer.
		 * On Windows that moment is DLL_PROCESS_DETACH, where the loader lock is held and letting go of a DirectWrite typeface calls back into a module the loader is already unwinding.
		 */
		State& state() {
			static State* shared = new State();
			return *shared;
		}
	} // namespace

	bool registerFontData(sk_sp<SkData> data) {
		if (!data || data->isEmpty()) {
			return false;
		}

		State&                            shared = state();
		const std::lock_guard<std::mutex> lock(shared.mutex);

		// The same file fetched twice arrives as two separate blobs, and handing both over would leave a lookup choosing between two identical families.
		// SkData compares sizes before bytes, so this stays cheap for the usual case of distinct fonts.
		for (const sk_sp<SkData>& entry : shared.files) {
			if (entry->equals(data.get())) {
				return false;
			}
		}
		shared.files.push_back(std::move(data));

		// Both layers are fixed once built, so the new file only reaches a lookup by way of a fresh pair.
		shared.registered.reset();
		shared.composed.reset();
		return true;
	}

	sk_sp<SkFontMgr> fontManager() {
		State&                            shared = state();
		const std::lock_guard<std::mutex> lock(shared.mutex);

		if (shared.composed != nullptr) {
			return shared.composed;
		}

		// Opening the platform stack walks every installed family, which is far too much work to repeat for each renderer.
		if (!shared.systemOpened) {
			shared.system       = createSystemFontManager();
			shared.systemOpened = true;
		}

		// Every registered file goes into a single layer rather than one layer each, because a lookup stops at the first layer carrying the family name.
		// Split across layers, a family's bold would sit below its regular and never be reached.
		if (shared.registered == nullptr && !shared.files.empty()) {
			shared.registered = createDataFontManager(SkSpan<sk_sp<SkData>>(shared.files));
		}

		// The registered layer sits on top, so a font the host supplied shadows the system family of that name while everything it does not carry still resolves.
		shared.composed = createCompositeFontManager({shared.registered, shared.system});
		return shared.composed;
	}
} // namespace music_lyric_player::backend::font
