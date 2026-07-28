#include "widgets.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "imgui.h"
#include "theme.h"

namespace example::widgets {
	namespace {
		namespace color = theme::color;

		// imgui_internal.h keeps its own copy, and this file has no other reason to reach into the internals.
		constexpr float kPi = 3.14159265358979323846f;

		// The padding a container reserves around the items inside it.
		// Indenting already narrows what ImGui reports as available, so only the right side has to be tracked, and the left only to undo it.
		struct Inset {
			float left  = 0.0f;
			float right = 0.0f;
		};

		std::vector<Inset> insets;

		/**
		 * Returns how far from the right edge the current container's items must stop.
		 */
		float insetRight() {
			return insets.empty() ? 0.0f : insets.back().right;
		}

		/**
		 * Converts a packed colour into the float quadruple ImGui's style stack takes.
		 */
		ImVec4 toVec4(ImU32 value) {
			return ImGui::ColorConvertU32ToFloat4(value);
		}

		/**
		 * Blends two packed colours channel by channel.
		 */
		ImU32 mix(ImU32 from, ImU32 to, float t) {
			const float  amount = std::clamp(t, 0.0f, 1.0f);
			const ImVec4 a      = toVec4(from);
			const ImVec4 b      = toVec4(to);
			return ImGui::ColorConvertFloat4ToU32(
				ImVec4(a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount, a.z + (b.z - a.z) * amount, a.w + (b.w - a.w) * amount));
		}

		/**
		 * Eases a value stored under `id` toward `target`, standing in for the transitions the stylesheet declares.
		 */
		float animate(ImGuiID id, float target, float duration) {
			float* stored = ImGui::GetStateStorage()->GetFloatRef(id, target);
			*stored       = approach(*stored, target, duration);
			return *stored;
		}

		/**
		 * Draws text with the face and size currently pushed.
		 */
		void drawText(ImDrawList* list, ImVec2 position, ImU32 tint, const char* text) {
			list->AddText(position, tint, text);
		}

		/**
		 * Maps the twenty-four unit grid the playground's icons are drawn on onto the panel's pixels, optionally turned about the icon's centre.
		 */
		struct Pen {
			ImDrawList* list      = nullptr;
			ImVec2      center    = ImVec2(0.0f, 0.0f);
			float       unit      = 1.0f;
			float       cosine    = 1.0f;
			float       sine      = 0.0f;
			ImU32       tint      = 0;
			float       thickness = 1.0f;

			/**
			 * Returns where a point of the icon grid lands on screen.
			 */
			ImVec2 at(float x, float y) const {
				const float dx = (x - 12.0f) * this->unit;
				const float dy = (y - 12.0f) * this->unit;
				return ImVec2(this->center.x + dx * this->cosine - dy * this->sine, this->center.y + dx * this->sine + dy * this->cosine);
			}

			/**
			 * Strokes a straight segment between two grid points.
			 */
			void line(float x1, float y1, float x2, float y2) const {
				this->list->AddLine(this->at(x1, y1), this->at(x2, y2), this->tint, this->thickness);
			}

			/**
			 * Strokes the polyline through `count` grid coordinate pairs, closing it back to the start when asked.
			 */
			void polyline(const float* points, int count, bool closed) const {
				for (int i = 0; i < count; ++i) {
					this->list->PathLineTo(this->at(points[i * 2], points[i * 2 + 1]));
				}
				this->list->PathStroke(this->tint, closed ? ImDrawFlags_Closed : ImDrawFlags_None, this->thickness);
			}

			/**
			 * Strokes an arc of `radius` grid units about a grid point, between two angles of the unturned grid.
			 */
			void arc(float x, float y, float radius, float from, float to) const {
				const float turn = std::atan2(this->sine, this->cosine);
				this->list->PathArcTo(this->at(x, y), radius * this->unit, from + turn, to + turn);
				this->list->PathStroke(this->tint, ImDrawFlags_None, this->thickness);
			}

			/**
			 * Fills the triangle through three grid points.
			 */
			void triangle(float x1, float y1, float x2, float y2, float x3, float y3) const {
				this->list->AddTriangleFilled(this->at(x1, y1), this->at(x2, y2), this->at(x3, y3), this->tint);
			}

			/**
			 * Fills a rounded rectangle spanning two grid points.
			 */
			void bar(float x1, float y1, float x2, float y2, float radius) const {
				this->list->AddRectFilled(this->at(x1, y1), this->at(x2, y2), this->tint, radius * this->unit);
			}

			/**
			 * Strokes a circle of `radius` grid units about a grid point.
			 */
			void circle(float x, float y, float radius) const {
				this->list->AddCircle(this->at(x, y), radius * this->unit, this->tint, 0, this->thickness);
			}
		};
	} // namespace

	float approach(float current, float target, float duration) {
		const float delta = ImGui::GetIO().DeltaTime;
		if (duration <= 0.0f || delta <= 0.0f) {
			return target;
		}

		const float step = 1.0f - std::pow(0.002f, delta / duration);
		return current + (target - current) * std::clamp(step, 0.0f, 1.0f);
	}

	void icon(ImDrawList* list, Icon which, ImVec2 center, float size, ImU32 tint, float rotation) {
		Pen pen;
		pen.list   = list;
		pen.center = center;
		pen.unit   = size / 24.0f;
		pen.cosine = std::cos(rotation);
		pen.sine   = std::sin(rotation);
		pen.tint   = tint;
		// Every glyph carries the two unit stroke the playground's SVGs declare, never thinner than one pixel.
		pen.thickness = std::max(1.0f, 2.0f * pen.unit);

		switch (which) {
		case Icon::Menu:
			pen.line(3.0f, 6.0f, 21.0f, 6.0f);
			pen.line(3.0f, 12.0f, 21.0f, 12.0f);
			pen.line(3.0f, 18.0f, 21.0f, 18.0f);
			break;

		case Icon::Play:
			pen.triangle(8.0f, 5.0f, 8.0f, 19.0f, 19.0f, 12.0f);
			break;

		case Icon::Pause:
			pen.bar(6.0f, 4.0f, 10.0f, 20.0f, 1.0f);
			pen.bar(14.0f, 4.0f, 18.0f, 20.0f, 1.0f);
			break;

		case Icon::Restart:
			// An almost closed ring whose opening sits at the top left, where the arrow head is drawn.
			pen.arc(12.0f, 12.0f, 9.0f, -0.60f * kPi, 1.15f * kPi);
			pen.line(3.0f, 3.0f, 3.0f, 8.0f);
			pen.line(3.0f, 8.0f, 8.0f, 8.0f);
			break;

		case Icon::Volume:
		case Icon::VolumeMuted: {
			const float speaker[] = {11.0f, 5.0f, 6.0f, 9.0f, 2.0f, 9.0f, 2.0f, 15.0f, 6.0f, 15.0f, 11.0f, 19.0f};
			pen.polyline(speaker, 6, true);
			if (which == Icon::Volume) {
				pen.arc(12.0f, 12.0f, 5.0f, -0.25f * kPi, 0.25f * kPi);
				pen.arc(12.0f, 12.0f, 10.0f, -0.25f * kPi, 0.25f * kPi);
			} else {
				pen.line(17.0f, 9.0f, 23.0f, 15.0f);
				pen.line(23.0f, 9.0f, 17.0f, 15.0f);
			}
			break;
		}

		case Icon::Music: {
			const float staff[] = {9.0f, 18.0f, 9.0f, 5.0f, 21.0f, 3.0f, 21.0f, 16.0f};
			pen.polyline(staff, 4, false);
			pen.circle(6.0f, 18.0f, 3.0f);
			pen.circle(18.0f, 16.0f, 3.0f);
			break;
		}

		case Icon::Lyric: {
			const float page[] = {5.0f, 3.0f, 19.0f, 3.0f, 19.0f, 21.0f, 5.0f, 21.0f};
			pen.polyline(page, 4, true);
			pen.line(9.0f, 8.0f, 15.0f, 8.0f);
			pen.line(9.0f, 12.0f, 15.0f, 12.0f);
			pen.line(9.0f, 16.0f, 13.0f, 16.0f);
			break;
		}

		case Icon::Chevron: {
			const float points[] = {9.0f, 18.0f, 15.0f, 12.0f, 9.0f, 6.0f};
			pen.polyline(points, 3, false);
			break;
		}

		case Icon::Check: {
			const float points[] = {20.0f, 6.0f, 9.0f, 17.0f, 4.0f, 12.0f};
			pen.polyline(points, 3, false);
			break;
		}
		}
	}

	bool iconButton(const char* id, Icon which, float box, bool active, bool disabled) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();
		const ImVec2          origin  = ImGui::GetCursorScreenPos();

		ImGui::BeginDisabled(disabled);
		const bool clicked = ImGui::InvisibleButton(id, ImVec2(box, box));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::EndDisabled();

		ImU32 tint = color::textSecondary;
		if (disabled) {
			tint = color::textMuted;
		} else if (hovered || active) {
			tint = color::primary;
			list->AddRectFilled(origin, ImVec2(origin.x + box, origin.y + box), color::primaryFaint, metrics.radiusMd);
		}
		icon(list, which, ImVec2(origin.x + box * 0.5f, origin.y + box * 0.5f), metrics.iconGlyph, tint);
		return clicked;
	}

	bool playButton(const char* id, bool playing, bool disabled) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();
		const ImVec2          origin  = ImGui::GetCursorScreenPos();
		const float           box     = metrics.playButton;

		ImGui::BeginDisabled(disabled);
		const bool clicked = ImGui::InvisibleButton(id, ImVec2(box, box));
		const bool hovered = ImGui::IsItemHovered();
		ImGui::EndDisabled();

		if (!disabled && hovered) {
			list->AddRectFilled(origin, ImVec2(origin.x + box, origin.y + box), color::primaryFaint, metrics.radiusMd);
		}
		icon(list,
			playing ? Icon::Pause : Icon::Play,
			ImVec2(origin.x + box * 0.5f, origin.y + box * 0.5f),
			metrics.iconGlyph,
			disabled ? color::textMuted : color::primary);
		return clicked;
	}

	float pillButtonWidth(const char* label) {
		const theme::Metrics& metrics = theme::metrics();

		ImGui::PushFont(nullptr, metrics.fontSmall);
		const float text = ImGui::CalcTextSize(label).x;
		ImGui::PopFont();
		return 26.0f * metrics.scale + 13.0f * metrics.scale + text;
	}

	bool pillButton(const char* id, Icon which, const char* label) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();

		ImGui::PushFont(nullptr, metrics.fontSmall);
		const ImVec2 textSize = ImGui::CalcTextSize(label);
		const float  glyph    = 13.0f * metrics.scale;
		const float  padding  = 10.0f * metrics.scale;
		const float  gap      = 6.0f * metrics.scale;
		const float  height   = metrics.fontSmall + 10.0f * metrics.scale;
		const float  width    = padding * 2.0f + glyph + gap + textSize.x;
		const ImVec2 origin   = ImGui::GetCursorScreenPos();
		const bool   clicked  = ImGui::InvisibleButton(id, ImVec2(width, height));
		const bool   hovered  = ImGui::IsItemHovered();

		const ImU32  fill   = hovered ? color::primaryFaint : color::background;
		const ImU32  stroke = hovered ? color::primaryBorder : color::border;
		const ImU32  tint   = hovered ? color::primaryStrong : color::textSecondary;
		const ImVec2 corner = ImVec2(origin.x + width, origin.y + height);
		list->AddRectFilled(origin, corner, fill, metrics.radiusSm);
		list->AddRect(origin, corner, stroke, metrics.radiusSm, ImDrawFlags_None, std::max(1.0f, metrics.scale));
		icon(list, which, ImVec2(origin.x + padding + glyph * 0.5f, origin.y + height * 0.5f), glyph, tint);
		drawText(list, ImVec2(origin.x + padding + glyph + gap, origin.y + (height - textSize.y) * 0.5f), tint, label);
		ImGui::PopFont();

		return clicked;
	}

	Drag slider(const char* id, float width, float value, float minimum, float maximum, bool disabled) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();

		Drag result;
		result.value = value;

		ImGui::PushID(id);
		const ImVec2 origin = ImGui::GetCursorScreenPos();

		ImGui::BeginDisabled(disabled);
		ImGui::InvisibleButton("##track", ImVec2(std::max(width, 1.0f), metrics.sliderHeight));
		const bool hovered = ImGui::IsItemHovered();
		const bool held    = ImGui::IsItemActive();
		result.released    = ImGui::IsItemDeactivated();
		ImGui::EndDisabled();

		// The handle follows the pointer for as long as it is held, so the host seeks continuously through a drag.
		if (held && width > 0.0f && maximum > minimum) {
			const float ratio = std::clamp((ImGui::GetIO().MousePos.x - origin.x) / width, 0.0f, 1.0f);
			const float next  = minimum + ratio * (maximum - minimum);
			if (next != value) {
				result.value   = next;
				result.changed = true;
			}
		}

		const float  alpha  = disabled ? 0.5f : 1.0f;
		const float  filled = maximum > minimum ? std::clamp((result.value - minimum) / (maximum - minimum), 0.0f, 1.0f) : 0.0f;
		const float  middle = origin.y + metrics.sliderHeight * 0.5f;
		const ImVec2 railMin(origin.x, middle - metrics.sliderTrack * 0.5f);
		const ImVec2 railMax(origin.x + width, middle + metrics.sliderTrack * 0.5f);
		list->AddRectFilled(railMin, railMax, mix(color::background, color::border, alpha), metrics.radiusSm);
		if (filled > 0.0f) {
			list->AddRectFilled(railMin, ImVec2(railMin.x + width * filled, railMax.y), mix(color::background, color::primary, alpha), metrics.radiusSm);
		}

		// The handle is scaled away at rest and grows in only once the pointer is over the track.
		const float grown = animate(ImGui::GetID("##thumb"), (hovered || held) && !disabled ? 1.0f : 0.0f, theme::motion::fast);
		if (grown > 0.01f) {
			list->AddCircleFilled(ImVec2(origin.x + width * filled, middle), metrics.sliderThumb * 0.5f * grown, color::primary);
		}
		ImGui::PopID();

		return result;
	}

	bool segmented(const char* id, const char* const* labels, int count, int* selected) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();
		if (count <= 0) {
			return false;
		}

		ImGui::PushID(id);
		const float  padding = 4.0f * metrics.scale;
		const float  gap     = 4.0f * metrics.scale;
		const float  width   = contentWidth();
		const float  height  = metrics.tabHeight + padding * 2.0f;
		const ImVec2 origin  = ImGui::GetCursorScreenPos();

		list->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), color::backgroundAlt, metrics.radiusMd);

		const float slot = (width - padding * 2.0f - gap * static_cast<float>(count - 1)) / static_cast<float>(count);

		// The indicator slides rather than jumps, the way the stylesheet transitions its transform.
		const float  position = animate(ImGui::GetID("##indicator"), static_cast<float>(*selected), theme::motion::base);
		const ImVec2 markMin(origin.x + padding + position * (slot + gap), origin.y + padding);
		const ImVec2 markMax(markMin.x + slot, markMin.y + metrics.tabHeight);
		list->AddRectFilled(ImVec2(markMin.x, markMin.y + metrics.scale), ImVec2(markMax.x, markMax.y + metrics.scale), color::shadow, metrics.radiusSm);
		list->AddRectFilled(markMin, markMax, color::background, metrics.radiusSm);

		bool changed = false;
		for (int i = 0; i < count; ++i) {
			ImGui::PushID(i);
			const ImVec2 corner(origin.x + padding + static_cast<float>(i) * (slot + gap), origin.y + padding);
			ImGui::SetCursorScreenPos(corner);
			if (ImGui::InvisibleButton("##tab", ImVec2(slot, metrics.tabHeight))) {
				*selected = i;
				changed   = true;
			}
			const bool  hovered = ImGui::IsItemHovered();
			const bool  current = i == *selected;
			const ImU32 tint    = current ? color::primaryStrong : (hovered ? color::text : color::textSecondary);

			ImGui::PushFont(current ? theme::semibold() : nullptr, metrics.fontBody);
			const ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
			drawText(list, ImVec2(corner.x + (slot - textSize.x) * 0.5f, corner.y + (metrics.tabHeight - textSize.y) * 0.5f), tint, labels[i]);
			ImGui::PopFont();
			ImGui::PopID();
		}

		// The buttons left the layout cursor mid-row, so the container is claimed as one item before moving on.
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy(ImVec2(width, height));
		ImGui::PopID();

		return changed;
	}

	bool toggle(const char* id, bool value, float rowHeight) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();

		const float width  = std::max(ImGui::CalcItemWidth(), metrics.toggleWidth);
		const float height = rowHeight > 0.0f ? rowHeight : metrics.controlHeight;
		ImGui::PushID(id);
		const ImVec2 origin  = ImGui::GetCursorScreenPos();
		const bool   clicked = ImGui::InvisibleButton("##switch", ImVec2(width, height));
		const bool   hovered = ImGui::IsItemHovered();

		// The switch is right aligned in the control column while the whole column stays clickable.
		const ImVec2 pillMin(origin.x + width - metrics.toggleWidth, origin.y + (height - metrics.toggleHeight) * 0.5f);
		const ImVec2 pillMax(pillMin.x + metrics.toggleWidth, pillMin.y + metrics.toggleHeight);

		const float moved = animate(ImGui::GetID("##thumb"), value ? 1.0f : 0.0f, theme::motion::fast);
		const ImU32 off   = hovered ? color::textMuted : color::borderStrong;
		const ImU32 on    = hovered ? color::primaryStrong : color::primary;
		list->AddRectFilled(pillMin, pillMax, mix(off, on, moved), metrics.radiusFull);

		const float  inset  = 2.0f * metrics.scale;
		const float  radius = metrics.toggleThumb * 0.5f;
		const float  travel = metrics.toggleWidth - metrics.toggleThumb - inset * 2.0f;
		const ImVec2 thumb(pillMin.x + inset + radius + travel * moved, (pillMin.y + pillMax.y) * 0.5f);
		list->AddCircleFilled(ImVec2(thumb.x, thumb.y + metrics.scale), radius, color::shadowThumb);
		list->AddCircleFilled(thumb, radius, color::textInverse);
		ImGui::PopID();

		return clicked;
	}

	int select(const char* id, const char* const* labels, int count, int selected, bool assigned) {
		const theme::Metrics& metrics = theme::metrics();
		const ImVec2          origin  = ImGui::GetCursorScreenPos();
		const float           width   = ImGui::CalcItemWidth();
		ImGui::SetNextItemWidth(width);

		const char* preview = selected >= 0 && selected < count ? labels[selected] : "";
		// An inherited value reads as a placeholder rather than as something the user set.
		if (!assigned) {
			ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color::textMuted));
		}
		// The popup's padding is read when it opens, so it has to be in effect around the whole combo.
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f * metrics.scale, 4.0f * metrics.scale));
		const bool open = ImGui::BeginCombo(id, preview, ImGuiComboFlags_NoArrowButton);
		if (!assigned) {
			ImGui::PopStyleColor();
		}

		int picked = -1;
		if (open) {
			for (int i = 0; i < count; ++i) {
				const bool current = i == selected;
				ImGui::PushID(i);
				if (current) {
					ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color::primaryStrong));
				}
				if (ImGui::Selectable(labels[i], false, ImGuiSelectableFlags_None, ImVec2(0.0f, metrics.controlHeight))) {
					picked = i;
				}
				if (current) {
					ImGui::PopStyleColor();
					const ImVec2 rowMin = ImGui::GetItemRectMin();
					const ImVec2 rowMax = ImGui::GetItemRectMax();
					const float  glyph  = 14.0f * metrics.scale;
					icon(ImGui::GetWindowDrawList(), Icon::Check, ImVec2(rowMax.x - glyph, (rowMin.y + rowMax.y) * 0.5f), glyph, color::primary);
					ImGui::SetItemDefaultFocus();
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		ImGui::PopStyleVar();

		// Drawn once the combo is closed so it lands on top of the frame rather than under it.
		const float glyph = 14.0f * metrics.scale;
		icon(ImGui::GetWindowDrawList(),
			Icon::Chevron,
			ImVec2(origin.x + width - glyph, origin.y + metrics.controlHeight * 0.5f),
			glyph,
			color::textMuted,
			open ? -kPi * 0.5f : kPi * 0.5f);

		return picked == selected ? -1 : picked;
	}

	bool picker(const char* id, Icon which, const char* label, const char* meta, bool filled) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();

		const float  padding = metrics.cardPadding;
		const float  gap     = 12.0f * metrics.scale;
		const float  width   = contentWidth();
		const float  height  = padding * 2.0f + metrics.pickerIcon;
		const ImVec2 origin  = ImGui::GetCursorScreenPos();

		const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
		const bool hovered = ImGui::IsItemHovered();

		const bool   lit    = hovered || filled;
		const ImVec2 corner = ImVec2(origin.x + width, origin.y + height);
		list->AddRectFilled(origin, corner, lit ? color::primaryFaint : color::backgroundSubtle, metrics.radiusMd);
		list->AddRect(origin, corner, lit ? color::primaryBorder : color::border, metrics.radiusMd, ImDrawFlags_None, std::max(1.0f, metrics.scale));

		const ImVec2 badge(origin.x + padding, origin.y + padding);
		list->AddRectFilled(badge, ImVec2(badge.x + metrics.pickerIcon, badge.y + metrics.pickerIcon), color::primarySoft, metrics.radiusSm);
		icon(list, which, ImVec2(badge.x + metrics.pickerIcon * 0.5f, badge.y + metrics.pickerIcon * 0.5f), metrics.iconGlyph, color::primary);

		const float textLeft  = badge.x + metrics.pickerIcon + gap;
		const float textRight = corner.x - padding;
		list->PushClipRect(ImVec2(textLeft, origin.y), ImVec2(textRight, corner.y), true);

		ImGui::PushFont(theme::semibold(), metrics.fontBody);
		const float labelHeight = ImGui::CalcTextSize(label).y;
		ImGui::PopFont();
		ImGui::PushFont(nullptr, metrics.fontSmall);
		const float metaHeight = ImGui::CalcTextSize(meta).y;
		ImGui::PopFont();

		const float top = origin.y + (height - labelHeight - metaHeight) * 0.5f;
		ImGui::PushFont(theme::semibold(), metrics.fontBody);
		drawText(list, ImVec2(textLeft, top), color::text, label);
		ImGui::PopFont();
		ImGui::PushFont(nullptr, metrics.fontSmall);
		drawText(list, ImVec2(textLeft, top + labelHeight), color::textSecondary, meta);
		ImGui::PopFont();

		list->PopClipRect();
		return clicked;
	}

	void heading(const char* label, const char* hint) {
		const theme::Metrics& metrics = theme::metrics();

		ImGui::PushFont(theme::semibold(), metrics.fontBody);
		ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color::text));
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
		ImGui::PopFont();

		if (hint == nullptr) {
			return;
		}

		ImGui::PushFont(nullptr, metrics.fontTiny);
		const float width = ImGui::CalcTextSize(hint).x;
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - insetRight() - width));
		ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color::textMuted));
		ImGui::TextUnformatted(hint);
		ImGui::PopStyleColor();
		ImGui::PopFont();
	}

	void body(const char* text, bool muted) {
		const theme::Metrics& metrics = theme::metrics();

		ImGui::PushFont(nullptr, metrics.fontSmall);
		ImGui::PushStyleColor(ImGuiCol_Text, toVec4(muted ? color::textMuted : color::textSecondary));
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + contentWidth());
		ImGui::TextUnformatted(text);
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
		ImGui::PopFont();
	}

	void rule() {
		const theme::Metrics& metrics   = theme::metrics();
		const float           thickness = std::max(1.0f, metrics.scale);
		const float           width     = contentWidth();
		const ImVec2          origin    = ImGui::GetCursorScreenPos();

		ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + thickness), color::borderSoft);
		ImGui::Dummy(ImVec2(width, thickness));
	}

	void beginSurface(const char* id, float height) {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		// Accordion rows carry their own padding and stack flush against each other.
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, toVec4(color::backgroundSubtle));
		ImGui::PushStyleColor(ImGuiCol_Border, toVec4(color::borderSoft));
		const float span = height > 0.0f ? height : ImGui::GetContentRegionAvail().y;
		ImGui::BeginChild(id, ImVec2(contentWidth(), std::max(span, 1.0f)), ImGuiChildFlags_Borders);
		insets.push_back(Inset{});
	}

	void endSurface() {
		insets.pop_back();
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
	}

	void beginCard(const char* id) {
		const theme::Metrics& metrics = theme::metrics();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(metrics.cardPadding, 10.0f * metrics.scale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * metrics.scale, 4.0f * metrics.scale));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, toVec4(color::backgroundSubtle));
		ImGui::PushStyleColor(ImGuiCol_Border, toVec4(color::borderSoft));
		ImGui::BeginChild(id, ImVec2(contentWidth(), 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar);
		insets.push_back(Inset{});
	}

	void endCard() {
		insets.pop_back();
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
	}

	void beginInset(float left, float right) {
		if (left > 0.0f) {
			ImGui::Indent(left);
		}
		insets.push_back(Inset{left, insetRight() + right});
	}

	void endInset() {
		const Inset inset = insets.back();
		insets.pop_back();
		if (inset.left > 0.0f) {
			ImGui::Unindent(inset.left);
		}
	}

	float contentWidth() {
		return std::max(1.0f, ImGui::GetContentRegionAvail().x - insetRight());
	}

	bool sectionHeader(const char* label, int level) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();

		ImGui::PushID(label);
		bool* open = ImGui::GetStateStorage()->GetBoolRef(ImGui::GetID("##open"), false);

		// The stylesheet steps the head's left padding in as the tree deepens.
		const float indent = level == 0 ? metrics.framePadding : (level == 1 ? 22.0f * metrics.scale : 26.0f * metrics.scale);
		const float size   = level == 0 ? metrics.fontBody : metrics.fontSmall;
		const float width  = contentWidth();
		const float height = size + metrics.sectionPadding * 2.0f;

		const ImVec2 origin = ImGui::GetCursorScreenPos();
		if (ImGui::InvisibleButton("##head", ImVec2(width, height))) {
			*open = !*open;
		}
		if (ImGui::IsItemHovered()) {
			list->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), color::backgroundAlt);
		}

		const float turn = animate(ImGui::GetID("##turn"), *open ? 1.0f : 0.0f, theme::motion::base) * kPi * 0.5f;
		icon(list, Icon::Chevron, ImVec2(origin.x + indent + metrics.chevron * 0.5f, origin.y + height * 0.5f), metrics.chevron, color::textMuted, turn);

		const ImU32 tint = level == 0 ? color::text : (*open ? color::primaryStrong : color::textSecondary);
		ImGui::PushFont(theme::semibold(), size);
		const float baseline = origin.y + (height - ImGui::CalcTextSize(label).y) * 0.5f;
		drawText(list, ImVec2(origin.x + indent + metrics.chevron + 8.0f * metrics.scale, baseline), tint, label);
		ImGui::PopFont();
		ImGui::PopID();

		return *open;
	}

	void groupTitle(const char* label) {
		const theme::Metrics& metrics = theme::metrics();

		// The stylesheet sets a group's caption in upper case; ImGui has no letter spacing to go with it.
		std::string upper(label);
		for (char& character : upper) {
			character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
		}

		ImGui::PushFont(theme::semibold(), metrics.fontTiny);
		ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color::textMuted));
		ImGui::TextUnformatted(upper.c_str());
		ImGui::PopStyleColor();
		ImGui::PopFont();
	}

	bool groupHeader(const char* label) {
		const theme::Metrics& metrics = theme::metrics();
		ImDrawList*           list    = ImGui::GetWindowDrawList();

		ImGui::PushID(label);
		bool* open = ImGui::GetStateStorage()->GetBoolRef(ImGui::GetID("##open"), false);

		const float  chevron = 11.0f * metrics.scale;
		const float  width   = contentWidth();
		const float  height  = std::max(metrics.fontTiny, chevron);
		const ImVec2 origin  = ImGui::GetCursorScreenPos();
		if (ImGui::InvisibleButton("##head", ImVec2(width, height))) {
			*open = !*open;
		}
		const bool hovered = ImGui::IsItemHovered();

		const float turn = animate(ImGui::GetID("##turn"), *open ? 1.0f : 0.0f, theme::motion::base) * kPi * 0.5f;
		icon(list, Icon::Chevron, ImVec2(origin.x + chevron * 0.5f, origin.y + height * 0.5f), chevron, color::textMuted, turn);

		std::string upper(label);
		for (char& character : upper) {
			character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
		}

		ImGui::PushFont(theme::semibold(), metrics.fontTiny);
		const float baseline = origin.y + (height - ImGui::CalcTextSize(upper.c_str()).y) * 0.5f;
		drawText(list, ImVec2(origin.x + chevron + 5.0f * metrics.scale, baseline), hovered ? color::textSecondary : color::textMuted, upper.c_str());
		ImGui::PopFont();
		ImGui::PopID();

		return *open;
	}

	Master groupMaster(const char* label, bool value) {
		const theme::Metrics& metrics = theme::metrics();

		Master result;
		result.open = value;

		ImGui::PushID(label);
		groupTitle(label);
		ImGui::SameLine();
		// The switch sits at the right edge of the card, so the caption keeps whatever is left.
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - metrics.toggleWidth));
		ImGui::SetNextItemWidth(metrics.toggleWidth);
		result.toggled = toggle("##master", value, metrics.toggleHeight);
		ImGui::PopID();

		return result;
	}

	void field(const char* label) {
		const theme::Metrics& metrics = theme::metrics();

		ImGui::AlignTextToFramePadding();
		ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color::textSecondary));
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
		ImGui::SameLine();

		const float left   = ImGui::GetCursorPosX();
		const float right  = left + ImGui::GetContentRegionAvail().x - insetRight();
		const float column = std::max(left, right - metrics.controlWidth);
		ImGui::SetCursorPosX(column);
		ImGui::SetNextItemWidth(std::max(right - column, 1.0f));
	}
} // namespace example::widgets
