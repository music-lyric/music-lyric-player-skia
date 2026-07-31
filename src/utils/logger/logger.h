#ifndef MUSIC_LYRIC_PLAYER_UTILS_LOGGER_LOGGER_H_
#define MUSIC_LYRIC_PLAYER_UTILS_LOGGER_LOGGER_H_

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <ctime>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

// Clang and GCC check printf arguments against the format string, counting the implicit `this` first, which puts the format at index 2.
#if defined(__GNUC__) || defined(__clang__)
#define MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE __attribute__((format(printf, 2, 3)))
#else
#define MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE
#endif

namespace music_lyric_player::utils {
	/**
	 * A tagged sink that writes each message as `[time] [LyricPlayer:tag] [level] message`, wherever the host platform expects to find it.
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

		/**
		 * Reports detail that only someone tracing this module wants to see.
		 */
		void debug(const char* format, ...) const MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE {
			std::va_list args;
			va_start(args, format);
			this->write(Level::Debug, format, args);
			va_end(args);
		}

	private:
		// The project the module tag belongs to, since these messages land in consoles owned by whoever embedded the library.
		static constexpr const char* kProject = "LyricPlayer";

		// Longer messages are truncated rather than heap allocated, since logging must not fail on the paths that report a failure.
		static constexpr int kMessageLimit = 1024;

		// Room for `YYYY-MM-DD HH:MM:SS` and its terminator.
		static constexpr int kTimeLimit = 20;

		enum class Level {
			Error,
			Warn,
			Info,
			Debug,
		};

		/**
		 * Returns the word `level` is written as, held to four characters so the column lines up.
		 * That is why `eror` and `dbug` are spelt short rather than misspelt.
		 */
		static const char* nameOf(Level level) {
			switch (level) {
			case Level::Error:
				return "E";
			case Level::Warn:
				return "W";
			case Level::Debug:
				return "D";
			}
			return "I";
		}

		/**
		 * Writes the current local time into `buffer` as `YYYY-MM-DD HH:MM:SS`.
		 */
		static void formatTime(char* buffer, std::size_t size) {
			const std::time_t now = std::time(nullptr);

			// The plain `localtime` hands back a shared buffer, which the render thread and the host thread would race over.
			std::tm parts{};
#if defined(_WIN32)
			localtime_s(&parts, &now);
#else
			localtime_r(&now, &parts);
#endif

			std::strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &parts);
		}

		/**
		 * Formats one message and writes it as a single line, which is where the platform difference lives.
		 */
		void write(Level level, const char* format, std::va_list args) const {
			char timestamp[kTimeLimit];
			formatTime(timestamp, sizeof(timestamp));

#if defined(__ANDROID__)
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
			case Level::Debug:
				priority = ANDROID_LOG_DEBUG;
				break;
			}

			// Logcat filters on the tag, so the project name claims it and the whole formatted line still goes into the text.
			char message[kMessageLimit];
			std::vsnprintf(message, sizeof(message), format, args);
			__android_log_print(priority, kProject, "[%s] [%s:%s] [%s] %s", timestamp, nameOf(level), kProject, this->tag, message);
#else
			// Only the levels reporting a problem take stderr, which is what sorts them onto console.error in the browser.
			std::FILE* stream = level == Level::Error || level == Level::Warn ? stderr : stdout;
			std::fprintf(stream, "[%s] [%s] [%s:%s] ", timestamp, nameOf(level), kProject, this->tag);
			std::vfprintf(stream, format, args);
			std::fputc('\n', stream);
#endif
		}

		const char* tag;
	};
} // namespace music_lyric_player::utils

#undef MUSIC_LYRIC_PLAYER_LOGGER_PRINTF_LIKE

#endif // MUSIC_LYRIC_PLAYER_UTILS_LOGGER_LOGGER_H_
