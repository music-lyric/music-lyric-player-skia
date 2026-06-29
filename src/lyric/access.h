#ifndef MUSIC_LYRIC_PLAYER_LYRIC_ACCESS_H_
#define MUSIC_LYRIC_PLAYER_LYRIC_ACCESS_H_

#include <optional>

#include "info.pb.h"
#include "line/content.pb.h"

namespace music_lyric_player {
	/**
	 * Returns a line's start time in ms, reading through the proto `body` oneof.
	 * A line carrying no timing resolves to 0.
	 */
	double lineStartMs(const ::lyric::Line& line);

	/**
	 * Returns a line's end time in ms, reading through the proto `body` oneof.
	 * A line carrying no timing resolves to 0.
	 */
	double lineEndMs(const ::lyric::Line& line);

	/**
	 * Finds the meta offset (ms) by scanning `info.metas` for the offset content.
	 * Returns nullopt when no offset meta is present.
	 */
	std::optional<double> metaOffsetMs(const ::lyric::Info& info);
} // namespace music_lyric_player

#endif
