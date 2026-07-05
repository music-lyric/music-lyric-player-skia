#include "sample_lyric.h"

#include <cstdint>
#include <string>

#include "info.h"

namespace example {
	namespace {
		/**
		 * Appends a line-timed normal line carrying a single word of text.
		 */
		void addLine(::lyric::Info& info, std::uint32_t startMs, std::uint32_t endMs, const std::string& text) {
			// The line's time range lives on `Line` itself (shared by the normal / interlude bodies).
			::lyric::Line* line = info.add_lines();
			::lyric::Time* time = line->mutable_time();
			time->set_start(startMs);
			time->set_end(endMs);

			// A normal line holds its sung words inside a `LineContent`.
			::lyric::LineContent* content = line->mutable_normal()->mutable_content();

			::lyric::Word*       word       = content->add_words();
			::lyric::WordNormal* wordNormal = word->mutable_normal();
			wordNormal->set_content(text);

			::lyric::Time* wordTime = wordNormal->mutable_time();
			wordTime->set_start(startMs);
			wordTime->set_end(endMs);
		}

		void addInterlude(::lyric::Info& info, std::uint32_t startMs, std::uint32_t endMs) {
			// Time is on `Line`; the interlude body carries no fields of its own.
			::lyric::Line* line = info.add_lines();
			::lyric::Time* time = line->mutable_time();
			time->set_start(startMs);
			time->set_end(endMs);

			line->mutable_interlude();
		}
	} // namespace

	::lyric::Info buildSampleLyric() {
		::lyric::Info info = ::music_lyric_model::makeInfo();
		info.set_type(::lyric::INFO_TYPE_VALID);
		info.set_timing(::lyric::INFO_TIMING_LINE);

		addInterlude(info, 0, 2500);
		addLine(info, 2500, 6000, "Skia renders these lyrics");
		addLine(info, 6000, 10000, "systemfont, no embedded typeface");
		addLine(info, 10000, 15000, "This is a deliberately long lyric line meant to overflow the container width so the paragraph wraps across several visual rows");
		addLine(info, 15000, 19000, "歌词渲染示例");
		addLine(info, 19000, 24000, "中英混排 mixed CJK and latin 换行测试 wrapping test");
		addInterlude(info, 24000, 27000);
		addLine(info, 27000, 31000, "the active line snaps to the anchor");
		addLine(info, 31000, 35000, "scrolling follows the timing engine");
		addLine(info, 35000, 40000, "no animation yet, just the basics");

		return info;
	}
} // namespace example
