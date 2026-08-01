#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <jni.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "backend/font/font.h"
#include "clock.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "music_lyric_model.h"
#include "playback/player.h"
#include "rendering/config/config.gen.glaze.h"
#include "rendering/config/config.h"
#include "rendering/renderer.h"
#include "surface.h"
#include "utils/config/property.glaze.h"
#include "utils/logger/logger.h"

namespace music_lyric_player::platform::android {
	namespace {
		constexpr utils::Logger logger{"AndroidJni"};

		constexpr const char* kFailureClass = "java/lang/IllegalStateException";

		// The scalar slots `nativeSnapshot` fills, in the order the Kotlin side reads them back.
		constexpr jsize kSnapshotPlaying = 0;
		constexpr jsize kSnapshotTime    = 1;
		constexpr jsize kSnapshotOffset  = 2;
		constexpr jsize kSnapshotActive  = 3;
		constexpr jsize kSnapshotSlots   = 4;

		/**
		 * The VM this library was loaded into, so a callback can find its own thread's environment.
		 */
		JavaVM* runtime = nullptr;

		/**
		 * Returns the calling thread's JNI environment, or null when this thread is not attached to the VM.
		 * Nothing here attaches one: every call arrives on a Java thread, which stays attached for as long as it runs.
		 */
		JNIEnv* currentEnv() {
			if (runtime == nullptr) {
				return nullptr;
			}

			JNIEnv* env = nullptr;
			if (runtime->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
				return nullptr;
			}
			return env;
		}

		/**
		 * Clears an exception a Kotlin callback left pending, which every later JNI call in the same frame would otherwise fail against.
		 */
		void clearPending(JNIEnv* env) {
			if (!env->ExceptionCheck()) {
				return;
			}

			env->ExceptionDescribe();
			env->ExceptionClear();
		}

		/**
		 * Raises `what` as a Java exception on return from the current entry point.
		 * A C++ exception must never unwind past the JNI boundary, so every entry point converts one here instead.
		 */
		void throwFailure(JNIEnv* env, const char* what) noexcept {
			logger.error("jni call failed: %s", what);

			// A pending exception would be replaced silently, and it makes the lookup below unsafe.
			clearPending(env);

			jclass type = env->FindClass(kFailureClass);
			if (type == nullptr) {
				env->ExceptionClear();
				return;
			}

			env->ThrowNew(type, what);
			env->DeleteLocalRef(type);
		}

		/**
		 * Runs `fn` and returns its result, turning any C++ exception into a Java one and answering the caller with `fallback`.
		 */
		template <typename Fn>
		std::invoke_result_t<Fn&> guard(JNIEnv* env, Fn&& fn, std::invoke_result_t<Fn&> fallback) noexcept {
			try {
				return fn();
			} catch (const std::exception& error) {
				throwFailure(env, error.what());
				return fallback;
			} catch (...) {
				throwFailure(env, "unknown exception");
				return fallback;
			}
		}

		/**
		 * Runs `fn` for a void-returning entry point, turning any C++ exception into a Java one.
		 */
		template <typename Fn>
		void guardVoid(JNIEnv* env, Fn&& fn) noexcept {
			try {
				fn();
			} catch (const std::exception& error) {
				throwFailure(env, error.what());
			} catch (...) {
				throwFailure(env, "unknown exception");
			}
		}

		/**
		 * Reads the object a handle points at, rejecting the zero a released handle is left as.
		 */
		template <typename T>
		T& fromHandle(jlong handle) {
			T* value = reinterpret_cast<T*>(static_cast<std::intptr_t>(handle));
			if (value == nullptr) {
				throw std::runtime_error("call on a released handle");
			}
			return *value;
		}

		/**
		 * Hands `value` to the Kotlin side as an opaque handle.
		 */
		template <typename T>
		jlong toHandle(T* value) {
			return static_cast<jlong>(reinterpret_cast<std::intptr_t>(value));
		}

		/**
		 * Releases the object a handle points at, taking a zero as nothing to do.
		 * An app tearing down from both its own `close()` and a surface callback releases twice, since neither path can tell that the other ran.
		 */
		template <typename T>
		void releaseHandle(jlong handle) {
			delete reinterpret_cast<T*>(static_cast<std::intptr_t>(handle));
		}

		/**
		 * Copies a Java byte array into a contiguous buffer, which is empty when the array is null or has no elements.
		 */
		std::vector<std::uint8_t> toBytes(JNIEnv* env, jbyteArray array) {
			if (array == nullptr) {
				return {};
			}

			const jsize size = env->GetArrayLength(array);
			if (size <= 0) {
				return {};
			}

			std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
			env->GetByteArrayRegion(array, 0, size, reinterpret_cast<jbyte*>(bytes.data()));
			return bytes;
		}

		/**
		 * Copies a Java string into a `std::string`, which is empty when the string is null.
		 */
		std::string toString(JNIEnv* env, jstring value) {
			if (value == nullptr) {
				return {};
			}

			const char* chars = env->GetStringUTFChars(value, nullptr);
			if (chars == nullptr) {
				return {};
			}

			std::string result(chars, static_cast<std::size_t>(env->GetStringUTFLength(value)));
			env->ReleaseStringUTFChars(value, chars);
			return result;
		}

		/**
		 * Looks a method up on `type`, clearing the error the VM raises for a missing one so the caller can report the whole set as one failure.
		 */
		jmethodID findMethod(JNIEnv* env, jclass type, const char* name, const char* signature) {
			jmethodID method = env->GetMethodID(type, name, signature);
			if (method == nullptr) {
				env->ExceptionClear();
			}
			return method;
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
		 * The app's handle on one timing engine, owning the clock it reads time from and the listener it reports back through.
		 * A renderer borrows this, so the app has to release its renderers before releasing this.
		 */
		class Player {
		public:
			/**
			 * Starts on steady time; `setExternalTime()` hands the clock over to the app.
			 * The events are subscribed once here and read the listener as it stands when they fire, so setting one twice leaves no stale subscription.
			 */
			Player() : player(this->clock) {
				this->player.onPlay.add([this](double time) { this->reportPlay(time); });
				this->player.onPause.add([this](double time) { this->reportPause(time); });
				this->player.onLinesUpdate.add(
					[this](const std::vector<int>& indices, int active, bool isSeek) { this->reportLinesUpdate(indices, active, isSeek); });
				this->player.onLyricUpdate.add([this](const music_lyric_model::parsed::Info& info) { this->reportLyricUpdate(info); });
			}

			~Player() {
				// The listener goes first: disposing pauses playback, and a released player should not call back into the app.
				this->setListener(currentEnv(), nullptr);
				this->player.dispose();
			}

			Player(const Player&)            = delete;
			Player& operator=(const Player&) = delete;

			/**
			 * Loads a lyric from the encoded bytes of a `parsed.Info` protobuf message.
			 */
			void updateLyric(const std::vector<std::uint8_t>& bytes) {
				this->player.updateLyric(music_lyric_model::parsed::decodeParsedInfo(bytes));
			}

			/**
			 * Starts or resumes playback from where it left off.
			 */
			void play() {
				this->player.play();
			}

			/**
			 * Seeks to `seek` ms and starts playing from there.
			 */
			void playFrom(double seek) {
				this->player.play(seek);
			}

			/**
			 * Pauses playback.
			 */
			void pause() {
				this->player.pause();
			}

			/**
			 * Advances active-line tracking one step; the render thread calls this once per frame.
			 */
			void tick() {
				this->player.tick();
			}

			/**
			 * Sets the user's temporary offset in ms and resyncs immediately.
			 */
			void updateTempOffset(double value) {
				this->player.updateTempOffset(value);
			}

			/**
			 * Feeds the player a time in ms from the app's own source, such as a `MediaPlayer`.
			 * The first call takes the clock over from steady time for good, after which a fresh time has to arrive before every `tick()`.
			 */
			void setExternalTime(double now) {
				this->clock.set(now);
			}

			/**
			 * Converts a content time to a playback time by removing the active offset.
			 */
			double convertContentTime(double contentTime) const {
				return this->player.convertContentTime(contentTime);
			}

			/**
			 * Writes the playback state into `state` and as many active line indices as `indices` holds, returning how many the player actually has.
			 * A return larger than that array's length means the caller has to grow it and ask again.
			 * One call rather than a getter each keeps the per-frame JNI cost down, and it is also what makes the values a single frame's.
			 */
			jint snapshot(JNIEnv* env, jdoubleArray state, jintArray indices) const {
				if (state == nullptr || env->GetArrayLength(state) < kSnapshotSlots) {
					throw std::runtime_error("snapshot needs a state array of four");
				}

				jdouble scalars[kSnapshotSlots];
				scalars[kSnapshotPlaying] = this->player.currentPlaying() ? 1.0 : 0.0;
				scalars[kSnapshotTime]    = this->player.currentTime();
				scalars[kSnapshotOffset]  = this->player.currentOffset();
				scalars[kSnapshotActive]  = this->player.currentActive();
				env->SetDoubleArrayRegion(state, 0, kSnapshotSlots, scalars);

				const std::vector<int> active   = this->player.currentIndex();
				const jsize            capacity = indices == nullptr ? 0 : env->GetArrayLength(indices);
				const jsize            written  = std::min(capacity, static_cast<jsize>(active.size()));
				if (written > 0) {
					env->SetIntArrayRegion(indices, 0, written, active.data());
				}
				return static_cast<jint>(active.size());
			}

			/**
			 * Takes `listener` as the object every playback event is reported to, releasing whichever one it replaces.
			 * Passing null only drops the current one, which is how an app stops being called back before it releases this player.
			 */
			void setListener(JNIEnv* env, jobject listener) {
				if (this->listener != nullptr && env != nullptr) {
					env->DeleteGlobalRef(this->listener);
				}
				this->listener      = nullptr;
				this->onPlay        = nullptr;
				this->onPause       = nullptr;
				this->onLinesUpdate = nullptr;
				this->onLyricUpdate = nullptr;

				if (listener == nullptr || env == nullptr) {
					return;
				}

				// The ids are resolved once, since the listener's class cannot change while the reference stands.
				jclass type         = env->GetObjectClass(listener);
				this->onPlay        = findMethod(env, type, "onPlay", "(D)V");
				this->onPause       = findMethod(env, type, "onPause", "(D)V");
				this->onLinesUpdate = findMethod(env, type, "onLinesUpdate", "([IIZ)V");
				this->onLyricUpdate = findMethod(env, type, "onLyricUpdate", "(Ljava/lang/String;ZI)V");
				env->DeleteLocalRef(type);

				if (this->onPlay == nullptr || this->onPause == nullptr || this->onLinesUpdate == nullptr || this->onLyricUpdate == nullptr) {
					throw std::runtime_error("listener does not carry the expected callbacks");
				}

				this->listener = env->NewGlobalRef(listener);
			}

			/**
			 * The timing engine a renderer binds itself to; not reachable from Kotlin.
			 */
			playback::Player& engine() {
				return this->player;
			}

		private:
			/**
			 * Returns the environment a callback can be made through, or null while there is nothing to call.
			 */
			JNIEnv* callbackEnv() const {
				return this->listener == nullptr ? nullptr : currentEnv();
			}

			/**
			 * Reports that playback started.
			 */
			void reportPlay(double time) {
				JNIEnv* env = this->callbackEnv();
				if (env == nullptr) {
					return;
				}

				env->CallVoidMethod(this->listener, this->onPlay, static_cast<jdouble>(time));
				clearPending(env);
			}

			/**
			 * Reports that playback paused.
			 */
			void reportPause(double time) {
				JNIEnv* env = this->callbackEnv();
				if (env == nullptr) {
					return;
				}

				env->CallVoidMethod(this->listener, this->onPause, static_cast<jdouble>(time));
				clearPending(env);
			}

			/**
			 * Reports the new set of active lines.
			 */
			void reportLinesUpdate(const std::vector<int>& indices, int active, bool isSeek) {
				JNIEnv* env = this->callbackEnv();
				if (env == nullptr) {
					return;
				}

				const jsize size  = static_cast<jsize>(indices.size());
				jintArray   array = env->NewIntArray(size);
				if (array == nullptr) {
					clearPending(env);
					return;
				}
				if (size > 0) {
					env->SetIntArrayRegion(array, 0, size, indices.data());
				}

				env->CallVoidMethod(this->listener, this->onLinesUpdate, array, static_cast<jint>(active), isSeek ? JNI_TRUE : JNI_FALSE);
				env->DeleteLocalRef(array);
				clearPending(env);
			}

			/**
			 * Reports a loaded lyric as the three facts the app cannot read off the bytes it handed in.
			 */
			void reportLyricUpdate(const music_lyric_model::parsed::Info& info) {
				JNIEnv* env = this->callbackEnv();
				if (env == nullptr) {
					return;
				}

				jstring version = env->NewStringUTF(info.version.c_str());
				if (version == nullptr) {
					clearPending(env);
					return;
				}

				const jboolean valid = info.type == music_lyric_model::parsed::InfoType::Valid ? JNI_TRUE : JNI_FALSE;
				env->CallVoidMethod(this->listener, this->onLyricUpdate, version, valid, static_cast<jint>(info.lines.size()));
				env->DeleteLocalRef(version);
				clearPending(env);
			}

			ManualClock      clock;
			playback::Player player;

			// The listener outlives the call that set it, so it is held globally along with its method ids.
			jobject   listener      = nullptr;
			jmethodID onPlay        = nullptr;
			jmethodID onPause       = nullptr;
			jmethodID onLinesUpdate = nullptr;
			jmethodID onLyricUpdate = nullptr;
		};

		/**
		 * The app's handle on one `Surface`, owning the native window, the GPU surface and the lyric renderer drawing into it.
		 * It borrows the player it was created from, which therefore has to outlive it.
		 */
		class Renderer {
		public:
			/**
			 * Creates a renderer painting `player`'s lyric into `target`, or null when no GPU backend could open that window.
			 * The caller owns the result and releases it by deleting it.
			 */
			static Renderer* create(JNIEnv* env, Player& player, jobject target) {
				if (target == nullptr) {
					throw std::runtime_error("renderer needs a surface");
				}

				// The window takes a reference of its own, so the app is free to drop its Surface; the release in dispose() ends it.
				ANativeWindow* window = ANativeWindow_fromSurface(env, target);
				if (window == nullptr) {
					throw std::runtime_error("the surface carries no native window");
				}

				std::unique_ptr<backend::gpu::Surface> surface = createSurface(window);
				if (surface == nullptr) {
					ANativeWindow_release(window);
					return nullptr;
				}
				return new Renderer(player, window, std::move(surface));
			}

			~Renderer() {
				this->dispose();
			}

			Renderer(const Renderer&)            = delete;
			Renderer& operator=(const Renderer&) = delete;

			/**
			 * Sets the ratio of physical pixels to layout pixels, which is what scales every size the config is written in.
			 */
			void setDevicePixelRatio(float devicePixelRatio) {
				this->devicePixelRatio = devicePixelRatio > 0.0f ? devicePixelRatio : 1.0f;
			}

			/**
			 * Reports that the window changed size, reading the new one off the window itself.
			 * The app knows the size too, but the window is the only source the swapchain agrees with.
			 */
			void handleResize() {
				if (this->surface == nullptr) {
					return;
				}

				this->surface->handleResize(ANativeWindow_getWidth(this->window), ANativeWindow_getHeight(this->window));
			}

			/**
			 * Paints one frame of the lyric into the window.
			 */
			void renderFrame() {
				if (this->surface == nullptr) {
					return;
				}

				this->surface->renderFrame([this](SkCanvas* canvas) {
					this->renderer.setViewport(this->surface->width(), this->surface->height(), this->devicePixelRatio);
					this->renderer.render(canvas);
				});
			}

			/**
			 * Merges a partial config, given as JSON, over the current one.
			 * Keys the JSON leaves out keep their current value, so this doubles as a sparse patch.
			 */
			void updateConfig(const std::string& json) {
				rendering::config::Root patch;
				if (const glz::error_ctx error = glz::read_json(patch, json)) {
					throw std::runtime_error("malformed config json");
				}
				this->renderer.config.merge(patch);
			}

			/**
			 * Detaches from the player and releases the GPU surface and the window backing it.
			 * The app reaches this by releasing the renderer, which it has to do before `surfaceDestroyed` returns.
			 */
			void dispose() {
				std::vector<Renderer*>& live = liveRenderers();
				live.erase(std::remove(live.begin(), live.end(), this), live.end());

				this->renderer.dispose();
				this->surface.reset();

				if (this->window != nullptr) {
					ANativeWindow_release(this->window);
					this->window = nullptr;
				}
			}

			/**
			 * Takes the font manager the registered set now resolves to; not reachable from Kotlin.
			 */
			void refreshFontManager() {
				this->renderer.setFontManager(backend::font::fontManager());
			}

		private:
			Renderer(Player& player, ANativeWindow* window, std::unique_ptr<backend::gpu::Surface> surface)
			    : window(window), surface(std::move(surface)), renderer(player.engine(), backend::font::fontManager(), player.engine().clock()) {
				liveRenderers().push_back(this);
			}

			ANativeWindow*                         window;
			std::unique_ptr<backend::gpu::Surface> surface;
			rendering::Renderer                    renderer;
			// No backend consumes the ratio, so it is carried here and handed to the renderer each frame.
			float devicePixelRatio = 1.0f;
		};

		/**
		 * Adds a font file to the process-wide set every renderer resolves families from.
		 * Returns false when the bytes are empty or that exact file is already registered, in which case nothing is rebuilt.
		 */
		bool registerFont(JNIEnv* env, jbyteArray bytes) {
			const std::vector<std::uint8_t> data = toBytes(env, bytes);
			if (data.empty()) {
				return false;
			}
			if (!backend::font::registerFontData(SkData::MakeWithCopy(data.data(), data.size()))) {
				return false;
			}

			// The set a font manager was built from is fixed once it exists, so every live renderer takes a new one.
			for (Renderer* renderer : liveRenderers()) {
				renderer->refreshFontManager();
			}
			return true;
		}
	} // namespace
} // namespace music_lyric_player::platform::android

namespace bridge = music_lyric_player::platform::android;

// Every entry below is a static native method taking its handle explicitly, so the Kotlin side declares them as `@JvmStatic external` on a companion.
// They all run on the single render thread the Kotlin side posts to, which is what lets the renderer registry and the font manager be reached without a lock.

/**
 * Caches the VM the library was loaded into, which every later callback needs to find its own thread's environment.
 */
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
	bridge::runtime = vm;
	return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jstring JNICALL Java_music_lyric_player_skia_NativeLibrary_nativeVersion(JNIEnv* env, jclass) {
	return env->NewStringUTF(MUSIC_LYRIC_PLAYER_VERSION);
}

extern "C" JNIEXPORT jboolean JNICALL Java_music_lyric_player_skia_NativeLibrary_nativeRegisterFont(JNIEnv* env, jclass, jbyteArray bytes) {
	return bridge::guard(env, [&]() -> jboolean { return bridge::registerFont(env, bytes) ? JNI_TRUE : JNI_FALSE; }, jboolean{JNI_FALSE});
}

extern "C" JNIEXPORT jlong JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeCreate(JNIEnv* env, jclass) {
	return bridge::guard(env, [&] { return bridge::toHandle(new bridge::Player()); }, jlong{0});
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeUpdateLyric(JNIEnv* env, jclass, jlong handle, jbyteArray bytes) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).updateLyric(bridge::toBytes(env, bytes)); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativePlay(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).play(); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativePlayFrom(JNIEnv* env, jclass, jlong handle, jdouble seek) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).playFrom(seek); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativePause(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).pause(); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeTick(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).tick(); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeUpdateTempOffset(JNIEnv* env, jclass, jlong handle, jdouble value) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).updateTempOffset(value); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeSetExternalTime(JNIEnv* env, jclass, jlong handle, jdouble now) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).setExternalTime(now); });
}

extern "C" JNIEXPORT jint JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeSnapshot(JNIEnv* env, jclass, jlong handle, jdoubleArray state, jintArray indices) {
	return bridge::guard(env, [&] { return bridge::fromHandle<bridge::Player>(handle).snapshot(env, state, indices); }, jint{0});
}

extern "C" JNIEXPORT jdouble JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeConvertContentTime(JNIEnv* env, jclass, jlong handle, jdouble contentTime) {
	return bridge::guard(env, [&] { return bridge::fromHandle<bridge::Player>(handle).convertContentTime(contentTime); }, jdouble{0.0});
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeSetListener(JNIEnv* env, jclass, jlong handle, jobject listener) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Player>(handle).setListener(env, listener); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricPlayer_nativeDispose(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::releaseHandle<bridge::Player>(handle); });
}

extern "C" JNIEXPORT jlong JNICALL Java_music_lyric_player_skia_LyricRenderer_nativeCreate(JNIEnv* env, jclass, jlong player, jobject target) {
	return bridge::guard(env, [&] { return bridge::toHandle(bridge::Renderer::create(env, bridge::fromHandle<bridge::Player>(player), target)); }, jlong{0});
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricRenderer_nativeSetDevicePixelRatio(JNIEnv* env, jclass, jlong handle, jfloat devicePixelRatio) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Renderer>(handle).setDevicePixelRatio(devicePixelRatio); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricRenderer_nativeResize(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Renderer>(handle).handleResize(); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricRenderer_nativeRenderFrame(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Renderer>(handle).renderFrame(); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricRenderer_nativeUpdateConfig(JNIEnv* env, jclass, jlong handle, jstring json) {
	bridge::guardVoid(env, [&] { bridge::fromHandle<bridge::Renderer>(handle).updateConfig(bridge::toString(env, json)); });
}

extern "C" JNIEXPORT void JNICALL Java_music_lyric_player_skia_LyricRenderer_nativeDispose(JNIEnv* env, jclass, jlong handle) {
	bridge::guardVoid(env, [&] { bridge::releaseHandle<bridge::Renderer>(handle); });
}
