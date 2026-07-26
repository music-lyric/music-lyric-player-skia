#include "theme.h"

#include <cstddef>
#include <filesystem>
#include <system_error>

#include "imgui.h"

namespace example::theme {
	namespace {
		// The stylesheet asks for Segoe UI first and falls back to a CJK family, so the panel builds the same stack
		// out of two sources: a latin face, and a CJK face merged in for the glyphs the first one lacks.
		constexpr const char* kLatinRegular[] = {
			"C:\\Windows\\Fonts\\segoeui.ttf",
			"C:\\Windows\\Fonts\\tahoma.ttf",
		};
		constexpr const char* kLatinSemibold[] = {
			"C:\\Windows\\Fonts\\seguisb.ttf",
			"C:\\Windows\\Fonts\\segoeuib.ttf",
			"C:\\Windows\\Fonts\\tahomabd.ttf",
		};
		constexpr const char* kCjkRegular[] = {
			"C:\\Windows\\Fonts\\msyh.ttc",
			"C:\\Windows\\Fonts\\msyhl.ttc",
			"C:\\Windows\\Fonts\\simhei.ttf",
		};
		constexpr const char* kCjkSemibold[] = {
			"C:\\Windows\\Fonts\\msyhbd.ttc",
			"C:\\Windows\\Fonts\\msyh.ttc",
			"C:\\Windows\\Fonts\\simhei.ttf",
		};

		Metrics  scaled;
		ImFont*  boldFace = nullptr;

		/**
		 * Multiplies every length the stylesheet declares by `scale`, since the panel lays out in physical pixels.
		 */
		Metrics scaleMetrics(float scale) {
			const Metrics base{};
			Metrics       result;
			result.scale = scale;

			result.sidebarWidth   = base.sidebarWidth * scale;
			result.controlsHeight = base.controlsHeight * scale;
			result.framePadding   = base.framePadding * scale;

			result.radiusXs   = base.radiusXs * scale;
			result.radiusSm   = base.radiusSm * scale;
			result.radiusMd   = base.radiusMd * scale;
			result.radiusLg   = base.radiusLg * scale;
			result.radiusFull = base.radiusFull * scale;

			result.fontTitle = base.fontTitle * scale;
			result.fontBody  = base.fontBody * scale;
			result.fontSmall = base.fontSmall * scale;
			result.fontTiny  = base.fontTiny * scale;

			result.controlHeight = base.controlHeight * scale;
			result.controlWidth  = base.controlWidth * scale;
			result.rowHeight     = base.rowHeight * scale;
			result.tabHeight     = base.tabHeight * scale;
			result.iconButton    = base.iconButton * scale;
			result.playButton    = base.playButton * scale;
			result.iconGlyph     = base.iconGlyph * scale;
			result.toggleWidth   = base.toggleWidth * scale;
			result.toggleHeight  = base.toggleHeight * scale;
			result.toggleThumb   = base.toggleThumb * scale;
			result.sliderHeight  = base.sliderHeight * scale;
			result.sliderTrack   = base.sliderTrack * scale;
			result.sliderThumb   = base.sliderThumb * scale;
			result.volumeWidth   = base.volumeWidth * scale;
			result.pickerIcon    = base.pickerIcon * scale;

			result.cardPadding    = base.cardPadding * scale;
			result.sectionPadding = base.sectionPadding * scale;
			result.sectionIndent  = base.sectionIndent * scale;
			result.chevron        = base.chevron * scale;
			return result;
		}

		/**
		 * Converts a packed colour into the float quadruple ImGui's style table stores.
		 */
		ImVec4 toVec4(ImU32 color) {
			return ImGui::ColorConvertU32ToFloat4(color);
		}

		/**
		 * Adds the first candidate that exists at `size`, merging it into the face last added when `merge` is set.
		 * Returns the face, or null when no candidate could be read.
		 */
		ImFont* addFace(ImGuiIO& io, const char* const* candidates, std::size_t count, float size, bool merge) {
			for (std::size_t i = 0; i < count; ++i) {
				std::error_code error;
				if (!std::filesystem::exists(candidates[i], error)) {
					continue;
				}

				ImFontConfig config;
				config.MergeMode = merge;
				// ImGui 1.92 rasterises glyphs on demand, so no range table is needed up front.
				if (ImFont* font = io.Fonts->AddFontFromFileTTF(candidates[i], size, &config)) {
					return font;
				}
			}
			return nullptr;
		}

		/**
		 * Builds the regular and semibold faces, each backed by a latin source with a CJK source merged behind it.
		 */
		void loadFonts(float size) {
			ImGuiIO& io = ImGui::GetIO();

			ImFont* regular = addFace(io, kLatinRegular, IM_ARRAYSIZE(kLatinRegular), size, false);
			if (regular != nullptr) {
				addFace(io, kCjkRegular, IM_ARRAYSIZE(kCjkRegular), size, true);
			} else {
				// Without a latin face the CJK family carries both scripts on its own.
				regular = addFace(io, kCjkRegular, IM_ARRAYSIZE(kCjkRegular), size, false);
			}
			if (regular == nullptr) {
				regular = io.Fonts->AddFontDefault();
			}
			io.FontDefault = regular;

			boldFace = addFace(io, kLatinSemibold, IM_ARRAYSIZE(kLatinSemibold), size, false);
			if (boldFace != nullptr) {
				addFace(io, kCjkSemibold, IM_ARRAYSIZE(kCjkSemibold), size, true);
			} else {
				boldFace = addFace(io, kCjkSemibold, IM_ARRAYSIZE(kCjkSemibold), size, false);
			}
			if (boldFace == nullptr) {
				boldFace = regular;
			}
		}

		/**
		 * Writes the playground's shapes and palette into the current context's style.
		 */
		void applyStyle() {
			ImGuiStyle& style = ImGui::GetStyle();

			style.WindowPadding     = ImVec2(scaled.framePadding, scaled.framePadding);
			style.WindowRounding    = 0.0f;
			style.WindowBorderSize  = 0.0f;
			style.ChildRounding     = scaled.radiusMd;
			style.ChildBorderSize   = 1.0f;
			style.PopupRounding     = scaled.radiusMd;
			style.PopupBorderSize   = 1.0f;
			// A frame is exactly one control tall, so the vertical padding is whatever the type leaves over.
			style.FramePadding      = ImVec2(10.0f * scaled.scale, (scaled.controlHeight - scaled.fontBody) * 0.5f);
			style.FrameRounding     = scaled.radiusSm;
			style.FrameBorderSize   = 1.0f;
			style.ItemSpacing       = ImVec2(8.0f * scaled.scale, 6.0f * scaled.scale);
			style.ItemInnerSpacing  = ImVec2(6.0f * scaled.scale, 6.0f * scaled.scale);
			style.CellPadding       = ImVec2(0.0f, 2.0f * scaled.scale);
			style.IndentSpacing     = scaled.sectionIndent;
			style.ScrollbarSize     = 10.0f * scaled.scale;
			style.ScrollbarRounding = scaled.radiusFull;
			style.GrabMinSize       = scaled.sliderThumb;
			style.GrabRounding      = scaled.radiusFull;
			style.TabRounding       = scaled.radiusSm;
			style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
			style.ButtonTextAlign     = ImVec2(0.5f, 0.5f);
			style.DisabledAlpha       = 0.5f;

			ImVec4* colors = style.Colors;
			colors[ImGuiCol_Text]           = toVec4(color::text);
			colors[ImGuiCol_TextDisabled]   = toVec4(color::textMuted);
			colors[ImGuiCol_WindowBg]       = toVec4(color::background);
			colors[ImGuiCol_ChildBg]        = toVec4(color::backgroundSubtle);
			colors[ImGuiCol_PopupBg]        = toVec4(color::background);
			colors[ImGuiCol_Border]         = toVec4(color::border);
			colors[ImGuiCol_BorderShadow]   = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			// A control keeps its white fill through every state; the stylesheet moves its border instead,
			// which the style table cannot express, so hover and focus lean on the faintest brand tints.
			colors[ImGuiCol_FrameBg]        = toVec4(color::background);
			colors[ImGuiCol_FrameBgHovered] = toVec4(color::backgroundAlt);
			colors[ImGuiCol_FrameBgActive]  = toVec4(color::primaryFaint);
			colors[ImGuiCol_Button]         = toVec4(color::background);
			colors[ImGuiCol_ButtonHovered]  = toVec4(color::primaryFaint);
			colors[ImGuiCol_ButtonActive]   = toVec4(color::primarySoft);
			colors[ImGuiCol_Header]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_HeaderHovered]  = toVec4(color::backgroundAlt);
			colors[ImGuiCol_HeaderActive]   = toVec4(color::surfaceHover);
			colors[ImGuiCol_Separator]      = toVec4(color::borderSoft);
			colors[ImGuiCol_SliderGrab]     = toVec4(color::primary);
			colors[ImGuiCol_SliderGrabActive] = toVec4(color::primaryStrong);
			colors[ImGuiCol_CheckMark]        = toVec4(color::primary);
			colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
			colors[ImGuiCol_ScrollbarGrab]        = toVec4(color::borderStrong);
			colors[ImGuiCol_ScrollbarGrabHovered] = toVec4(color::textMuted);
			colors[ImGuiCol_ScrollbarGrabActive]  = toVec4(color::textMuted);
			colors[ImGuiCol_TextSelectedBg]       = toVec4(color::primarySoft);
			colors[ImGuiCol_NavCursor]            = toVec4(color::primaryBorder);
		}
	} // namespace

	void apply(float scale) {
		scaled = scaleMetrics(scale > 0.0f ? scale : 1.0f);
		loadFonts(scaled.fontBody);
		applyStyle();
	}

	const Metrics& metrics() {
		return scaled;
	}

	ImFont* semibold() {
		return boldFace;
	}
} // namespace example::theme
