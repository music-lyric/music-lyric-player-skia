#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_WIDGETS_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_WIDGETS_H_

#include "imgui.h"

namespace example::widgets {
	/**
	 * The stroke glyphs the playground inlines as SVG, redrawn as draw-list paths on a 24 unit grid.
	 */
	enum class Icon {
		Menu,
		Play,
		Pause,
		Restart,
		Volume,
		VolumeMuted,
		Music,
		Lyric,
		Chevron,
		Check,
	};

	/**
	 * What one frame of dragging a slider reports back.
	 * A drag reports every frame the handle moves, and separately the frame it is let go.
	 */
	struct Drag {
		bool  changed  = false;
		bool  released = false;
		float value    = 0.0f;
	};

	/**
	 * What a card's title row reports when a switch owns it: whether the switch was clicked,
	 * and whether the rest of the card follows below.
	 */
	struct Master {
		bool toggled = false;
		bool open    = false;
	};

	/**
	 * Eases `current` one frame's worth toward `target`, standing in for the transitions the stylesheet declares.
	 * The approach is exponential, so it lands within a fraction of a pixel well inside `duration` seconds.
	 */
	float approach(float current, float target, float duration);

	/**
	 * Paints `which` centred on `center`, fitted to a `size` pixel box and turned by `rotation` radians.
	 */
	void icon(ImDrawList* list, Icon which, ImVec2 center, float size, ImU32 color, float rotation = 0.0f);

	/**
	 * Draws a square, borderless icon button that takes the brand tint once the pointer is over it.
	 */
	bool iconButton(const char* id, Icon which, float box, bool active = false, bool disabled = false);

	/**
	 * Draws the transport's larger play button, which already sits in the brand colour at rest.
	 */
	bool playButton(const char* id, bool playing, bool disabled);

	/**
	 * Draws the small outlined button a section header carries, an icon beside a label.
	 */
	bool pillButton(const char* id, Icon which, const char* label);

	/**
	 * Returns the width `pillButton` takes for `label`, so a header can right-align it before drawing.
	 */
	float pillButtonWidth(const char* label);

	/**
	 * Draws the thin slider: a three pixel rail whose round handle only grows once the pointer is over it.
	 */
	Drag slider(const char* id, float width, float value, float min, float max, bool disabled);

	/**
	 * Draws the segmented control the sidebar switches tabs with, sliding a lifted pill under the active one.
	 */
	bool segmented(const char* id, const char* const* labels, int count, int* selected);

	/**
	 * Draws the pill switch a boolean leaf is edited through, right aligned in the control column.
	 * `rowHeight` gives the switch a shorter row than a control when it heads a card instead of filling one.
	 */
	bool toggle(const char* id, bool value, float rowHeight = 0.0f);

	/**
	 * Draws the select trigger and its menu, returning the option picked or -1 while the selection stands.
	 * An unassigned leaf shows its inherited value in the muted tone, the way a placeholder reads.
	 */
	int select(const char* id, const char* const* labels, int count, int selected, bool assigned);

	/**
	 * Draws the large source card a tab picks a file with, tinted once one is loaded.
	 */
	bool picker(const char* id, Icon which, const char* label, const char* meta, bool filled);

	/**
	 * Draws a tab's section title, optionally trailed by a muted hint on the same row.
	 */
	void heading(const char* label, const char* hint = nullptr);

	/**
	 * Draws a run of body text, in the secondary tone by default and the muted one when it stands in for a value.
	 */
	void body(const char* text, bool muted = false);

	/**
	 * Draws a hairline rule in the soft border tone across the width the container leaves.
	 */
	void rule();

	/**
	 * Opens the rounded, hairlined surface an accordion lives inside.
	 * A height of zero or less fills whatever the tab has left below the cursor.
	 */
	void beginSurface(const char* id, float height = 0.0f);
	void endSurface();

	/**
	 * Opens a card holding one group of fields.
	 */
	void beginCard(const char* id);
	void endCard();

	/**
	 * Reserves `left` and `right` pixels around the items that follow, standing in for a container's padding.
	 */
	void beginInset(float left, float right);
	void endInset();

	/**
	 * Returns the width the container leaves for a full width item, with the inset in effect taken off.
	 */
	float contentWidth();

	/**
	 * Draws an accordion row at `level` and reports whether its body should follow.
	 * The open state lives in ImGui's per-window storage, so the caller keeps none of its own.
	 */
	bool sectionHeader(const char* label, int level);

	/**
	 * Draws a card's caption, upper-cased the way the stylesheet sets it.
	 */
	void groupTitle(const char* label);

	/**
	 * Draws a card's caption as a fold header and reports whether its body should follow.
	 */
	bool groupHeader(const char* label);

	/**
	 * Draws a card's caption beside the switch that owns the rest of the card.
	 */
	Master groupMaster(const char* label, bool value);

	/**
	 * Opens a settings row: the label on the left, the control column sized and reserved on the right.
	 * The control drawn next takes the width the row leaves it.
	 */
	void field(const char* label);
} // namespace example::widgets

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_WIDGETS_H_
