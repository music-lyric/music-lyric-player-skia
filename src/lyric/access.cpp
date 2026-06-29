#include "lyric/access.h"

#include "meta/meta.pb.h"

namespace music_lyric_player {
	double lineStartMs(const ::lyric::Line& line) {
		switch (line.body_case()) {
		case ::lyric::Line::kNormal:
			return line.normal().has_time() ? static_cast<double>(line.normal().time().start()) : 0.0;
		case ::lyric::Line::kInterlude:
			return line.interlude().has_time() ? static_cast<double>(line.interlude().time().start()) : 0.0;
		default:
			return 0.0;
		}
	}

	double lineEndMs(const ::lyric::Line& line) {
		switch (line.body_case()) {
		case ::lyric::Line::kNormal:
			return line.normal().has_time() ? static_cast<double>(line.normal().time().end()) : 0.0;
		case ::lyric::Line::kInterlude:
			return line.interlude().has_time() ? static_cast<double>(line.interlude().time().end()) : 0.0;
		default:
			return 0.0;
		}
	}

	std::optional<double> metaOffsetMs(const ::lyric::Info& info) {
		for (int i = 0; i < info.metas_size(); ++i) {
			const ::lyric::MetaItem& meta = info.metas(i);
			if (meta.content_case() == ::lyric::MetaItem::kOffset) {
				return meta.offset();
			}
		}
		return std::nullopt;
	}
} // namespace music_lyric_player
