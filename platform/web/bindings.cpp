#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "backend/font/font.h"
#include "backend/font/registry.h"
#include "backend/gpu/webgl/webgl.h"
#include "clock.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "music_lyric_model.h"
#include "playback/player.h"
#include "rendering/config/config.gen.glaze.h"
#include "rendering/config/config.h"
#include "rendering/renderer.h"
#include "utils/config/property.glaze.h"

namespace music_lyric_player::platform::web {
	namespace {
		/**
		 * Throws `message` into JS as a real `Error`.
		 * A C++ exception left to unwind out of the module reaches JS as a bare `WebAssembly.Exception`, which carries no message in a release build.
		 * The boundary converts it here instead.
		 */
		[[noreturn]] void throwJsError(const std::string& message) {
			emscripten::val::global("Error").new_("[music-lyric-player] " + message).throw_();
		}

		/**
		 * Runs `fn` and turns anything that escapes it into a JS `Error`, so no exception crosses into JS raw.
		 * The message is carried out of the handler before being thrown, so the JS exception never unwinds through a C++ catch block that is still handling one.
		 */
		template <typename Fn>
		std::invoke_result_t<Fn&> guard(Fn&& fn) {
			std::string failure;
			try {
				return fn();
			} catch (const std::exception& error) {
				failure = error.what();
			} catch (...) {
				failure = "unknown exception";
			}
			throwJsError(failure);
		}

		/**
		 * Copies a JS typed array, or any array of byte values, into a contiguous buffer.
		 */
		std::vector<std::uint8_t> toBytes(const emscripten::val& value) {
			return emscripten::convertJSArrayToNumberVector<std::uint8_t>(value);
		}

		class Renderer;

		/**
		 * Every renderer currently alive, so a font registered later can be pushed into all of them.
		 */
		std::vector<Renderer*>& liveRenderers() {
			static std::vector<Renderer*> renderers;
			return renderers;
		}

		/**
		 * The page's handle on one timing engine, owning the clock it reads time from.
		 * A renderer borrows this, so the page has to release its renderers before releasing this.
		 */
		class Player {
		public:
			/**
			 * Starts on steady time; `setExternalTime()` hands the clock over to the page.
			 */
			Player() : player(this->clock) {}

			Player(const Player&)            = delete;
			Player& operator=(const Player&) = delete;

			/**
			 * Loads a lyric from the encoded bytes of a `parsed.Info` protobuf message.
			 */
			void updateLyric(const emscripten::val& bytes) {
				guard([&] { this->player.updateLyric(music_lyric_model::parsed::decodeParsedInfo(toBytes(bytes))); });
			}

			/**
			 * Starts or resumes playback from where it left off.
			 */
			void play() {
				guard([&] { this->player.play(); });
			}

			/**
			 * Seeks to `seek` ms and starts playing from there.
			 */
			void playFrom(double seek) {
				guard([&] { this->player.play(seek); });
			}

			/**
			 * Pauses playback.
			 */
			void pause() {
				guard([&] { this->player.pause(); });
			}

			/**
			 * Advances active-line tracking one step; the page calls this once per animation frame.
			 */
			void tick() {
				guard([&] { this->player.tick(); });
			}

			/**
			 * Stops playback, drops the lyric and releases every registered callback.
			 */
			void dispose() {
				guard([&] { this->player.dispose(); });
			}

			/**
			 * Sets the user's temporary offset in ms and resyncs immediately.
			 */
			void updateTempOffset(double value) {
				guard([&] { this->player.updateTempOffset(value); });
			}

			/**
			 * Feeds the player a time in ms from the page's own source, such as an `<audio>` element.
			 * The first call takes the clock over from steady time for good.
			 * From then on the page owns the play head and has to push a fresh time before every `tick()`.
			 */
			void setExternalTime(double now) {
				guard([&] { this->clock.set(now); });
			}

			/**
			 * Whether playback is currently running.
			 */
			bool currentPlaying() const {
				return guard([&] { return this->player.currentPlaying(); });
			}

			/**
			 * The current playback time in ms.
			 */
			double currentTime() const {
				return guard([&] { return this->player.currentTime(); });
			}

			/**
			 * The combined effective offset in ms.
			 */
			double currentOffset() const {
				return guard([&] { return this->player.currentOffset(); });
			}

			/**
			 * The index of the primary active line, or -1 when none.
			 */
			int currentActive() const {
				return guard([&] { return this->player.currentActive(); });
			}

			/**
			 * The indices of every currently active line, as a JS array.
			 */
			emscripten::val currentIndex() const {
				return guard([&] { return emscripten::val::array(this->player.currentIndex()); });
			}

			/**
			 * Converts a content time to a playback time by removing the active offset.
			 */
			double convertContentTime(double contentTime) const {
				return guard([&] { return this->player.convertContentTime(contentTime); });
			}

			/**
			 * Calls `callback(time)` whenever playback starts.
			 */
			void onPlay(emscripten::val callback) {
				guard([&] { this->player.onPlay.add([callback](double time) { callback(time); }); });
			}

			/**
			 * Calls `callback(time)` whenever playback pauses.
			 */
			void onPause(emscripten::val callback) {
				guard([&] { this->player.onPause.add([callback](double time) { callback(time); }); });
			}

			/**
			 * Calls `callback(indices, active, isSeek)` whenever the set of active lines changes.
			 */
			void onLinesUpdate(emscripten::val callback) {
				guard([&] {
					this->player.onLinesUpdate.add([callback](const std::vector<int>& indices, int active, bool isSeek) {
						callback(emscripten::val::array(indices), active, isSeek);
					});
				});
			}

			/**
			 * Calls `callback(summary)` whenever a lyric loads, where the summary carries the schema version, whether it parsed and how many lines it holds.
			 * The parsed model is not marshalled back out, since the page already holds the bytes it handed in.
			 */
			void onLyricUpdate(emscripten::val callback) {
				guard([&] {
					this->player.onLyricUpdate.add([callback](const music_lyric_model::parsed::Info& info) {
						emscripten::val summary = emscripten::val::object();
						summary.set("version", info.version);
						summary.set("valid", info.type == music_lyric_model::parsed::InfoType::Valid);
						summary.set("lineCount", static_cast<int>(info.lines.size()));
						callback(summary);
					});
				});
			}

			/**
			 * The timing engine a renderer binds itself to; not exposed to JS.
			 */
			playback::Player& engine() {
				return this->player;
			}

		private:
			ManualClock      clock;
			playback::Player player;
		};

		/**
		 * The page's handle on one canvas, owning the GPU surface and the lyric renderer drawing into it.
		 * It borrows the player it was created from, which therefore has to outlive it.
		 */
		class Renderer {
		public:
			/**
			 * Creates a renderer painting `player`'s lyric onto the canvas `selector` matches, or null when the GPU stack cannot start.
			 * The caller owns the result and releases it through the binding's `delete()`.
			 */
			static Renderer* create(Player* player, const std::string& selector) {
				return guard([&]() -> Renderer* {
					if (player == nullptr) {
						throwJsError("renderer needs a player");
					}

					backend::gpu::NativeWindow window;
					window.selector = selector.c_str();

					std::unique_ptr<backend::gpu::Surface> surface = backend::gpu::webgl::createSurface(window);
					if (surface == nullptr) {
						return nullptr;
					}
					return new Renderer(*player, std::move(surface));
				});
			}

			~Renderer() {
				dispose();
			}

			Renderer(const Renderer&)            = delete;
			Renderer& operator=(const Renderer&) = delete;

			/**
			 * Resizes the canvas backbuffer to `width` by `height` physical pixels, at `devicePixelRatio`.
			 */
			void configure(int width, int height, float devicePixelRatio) {
				guard([&] {
					this->devicePixelRatio = devicePixelRatio > 0.0f ? devicePixelRatio : 1.0f;
					if (this->surface != nullptr) {
						this->surface->handleResize(width, height);
					}
				});
			}

			/**
			 * Paints one frame of the lyric into the canvas.
			 */
			void renderFrame() {
				guard([&] {
					if (this->surface == nullptr) {
						return;
					}
					this->surface->renderFrame([this](SkCanvas* canvas) {
						this->renderer.setViewport(this->surface->width(), this->surface->height(), this->devicePixelRatio);
						this->renderer.render(canvas);
					});
				});
			}

			/**
			 * Merges a partial config, given as JSON, over the current one.
			 * Keys the JSON leaves out keep their current value, so this doubles as a sparse patch.
			 */
			void updateConfig(const std::string& json) {
				guard([&] {
					rendering::config::Root patch;
					if (const glz::error_ctx error = glz::read_json(patch, json)) {
						throwJsError("malformed config json");
					}
					this->renderer.config.merge(patch);
				});
			}

			/**
			 * Detaches from the player and drops the GPU surface, leaving the object safe to release afterwards.
			 * Releasing the JS handle with `delete()` does this too.
			 * Calling it early lets the page order its teardown against the player it borrows.
			 */
			void dispose() {
				std::vector<Renderer*>& live = liveRenderers();
				live.erase(std::remove(live.begin(), live.end(), this), live.end());

				this->renderer.dispose();
				this->surface.reset();
			}

			/**
			 * Rebuilds the font manager from the registry; not exposed to JS.
			 */
			void refreshFontMgr() {
				this->renderer.setFontMgr(backend::font::createFontMgr());
			}

		private:
			Renderer(Player& player, std::unique_ptr<backend::gpu::Surface> surface)
			    : surface(std::move(surface)), renderer(player.engine(), backend::font::createFontMgr(), player.engine().clock()) {
				liveRenderers().push_back(this);
			}

			std::unique_ptr<backend::gpu::Surface> surface;
			rendering::Renderer                    renderer;
			// The page reports the ratio with every resize and no backend consumes it, so it is carried here and handed to the renderer each frame.
			float devicePixelRatio = 1.0f;
		};

		/**
		 * Adds a font file to the process-wide set every renderer resolves families from.
		 * Returns false when the bytes are empty or that exact file is already registered, in which case nothing is rebuilt.
		 */
		bool registerFont(const emscripten::val& bytes) {
			return guard([&] {
				const std::vector<std::uint8_t> data = toBytes(bytes);
				if (data.empty()) {
					return false;
				}
				if (!backend::font::registerFontData(SkData::MakeWithCopy(data.data(), data.size()))) {
					return false;
				}

				// The set a font manager was built from is fixed once it exists, so every live renderer takes a new one.
				for (Renderer* renderer : liveRenderers()) {
					renderer->refreshFontMgr();
				}
				return true;
			});
		}

		/**
		 * Returns the version this module was built from.
		 */
		std::string version() {
			return MUSIC_LYRIC_PLAYER_VERSION;
		}
	} // namespace
} // namespace music_lyric_player::platform::web

EMSCRIPTEN_BINDINGS(music_lyric_player) {
	using namespace music_lyric_player::platform::web;

	emscripten::class_<Player>("Player")
		.constructor<>()
		.function("updateLyric", &Player::updateLyric)
		.function("play", &Player::play)
		.function("playFrom", &Player::playFrom)
		.function("pause", &Player::pause)
		.function("tick", &Player::tick)
		.function("dispose", &Player::dispose)
		.function("updateTempOffset", &Player::updateTempOffset)
		.function("setExternalTime", &Player::setExternalTime)
		.function("currentIndex", &Player::currentIndex)
		.function("convertContentTime", &Player::convertContentTime)
		.function("onPlay", &Player::onPlay)
		.function("onPause", &Player::onPause)
		.function("onLinesUpdate", &Player::onLinesUpdate)
		.function("onLyricUpdate", &Player::onLyricUpdate)
		.property("currentPlaying", &Player::currentPlaying)
		.property("currentTime", &Player::currentTime)
		.property("currentOffset", &Player::currentOffset)
		.property("currentActive", &Player::currentActive);

	emscripten::class_<Renderer>("Renderer")
		.class_function("create", &Renderer::create, emscripten::allow_raw_pointers())
		.function("configure", &Renderer::configure)
		.function("renderFrame", &Renderer::renderFrame)
		.function("updateConfig", &Renderer::updateConfig)
		.function("dispose", &Renderer::dispose);

	emscripten::function("registerFont", &registerFont);
	emscripten::function("version", &version);
}
