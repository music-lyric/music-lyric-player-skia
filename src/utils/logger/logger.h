#ifndef MUSIC_LYRIC_PLAYER_UTILS_LOGGER_LOGGER_H_
#define MUSIC_LYRIC_PLAYER_UTILS_LOGGER_LOGGER_H_

#include <cstdarg>

#if defined(__ANDROID__)
#include <android/log.h>
#else
#include <cstdio>
#endif

// Clang and GCC check printf arguments against the format string, counting the implicit `this` first, which puts the format at index 2.
#if defined(__GNUC__) || defined(__clang__)
#define MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE __attribute__((format(printf, 2, 3)))
#else
#define MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE
#endif

namespace music_lyric_player::utils {
	/**
	 * A tagged sink that writes each message where the host platform expects to find it.
	 * Android has to use logcat because its processes wire stdout and stderr to /dev/null.
	 */
	class Logger {
	public:
		/**
		 * Creates a logger tagging every message with `tag`, which is borrowed and has to outlive it.
		 */
		explicit constexpr Logger(const char* tag) : tag(tag) {}

		/**
		 * Reports that something the caller needed did not work.
		 */
		void error(const char* format, ...) const MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE {
			std::va_list args;
			va_start(args, format);
			this->write(Level::Error, format, args);
			va_end(args);
		}

		/**
		 * Reports something suspect that the caller recovered from.
		 */
		void warn(const char* format, ...) const MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE {
			std::va_list args;
			va_start(args, format);
			this->write(Level::Warn, format, args);
			va_end(args);
		}

		/**
		 * Reports something worth knowing that is not a problem.
		 */
		void info(const char* format, ...) const MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE {
			std::va_list args;
			va_start(args, format);
			this->write(Level::Info, format, args);
			va_end(args);
		}

	private:
		enum class Level {
			Error,
			Warn,
			Info,
		};

		/**
		 * Formats one message and writes it as a single line, which is where the platform difference lives.
		 */
		void write(Level level, const char* format, std::va_list args) const {
#if defined(__ANDROID__)
			// Logcat carries both the level and the tag, so the text itself stays bare.
			int priority = ANDROID_LOG_INFO;
			switch (level) {
			case Level::Error:
				priority = ANDROID_LOG_ERROR;
				break;
			case Level::Warn:
				priority = ANDROID_LOG_WARN;
				break;
			case Level::Info:
				break;
			}

			__android_log_vprint(priority, this->tag, format, args);
#else
			// Only the levels reporting a problem take stderr, which is what sorts them onto console.error in the browser.
			const char* name   = "info";
			std::FILE*  stream = stdout;
			switch (level) {
			case Level::Error:
				name   = "error";
				stream = stderr;
				break;
			case Level::Warn:
				name   = "warn";
				stream = stderr;
				break;
			case Level::Info:
				break;
			}

			std::fprintf(stream, "[%s] %s: ", this->tag, name);
			std::vfprintf(stream, format, args);
			std::fputc('\n', stream);
#endif
		}

		const char* tag;
	};
} // namespace music_lyric_player::utils

#undef MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE

#endif // MUSIC_LYRIC_PLAYER_UTILS_LOGGER_LOGGER_H_
