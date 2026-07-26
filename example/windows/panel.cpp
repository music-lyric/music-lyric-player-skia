#include "panel.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <system_error>

#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "imgui_skia.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "settings.h"

namespace example {
	namespace {
		// Chinese track names and lyric lines are common, so prefer a system font that covers CJK.
		constexpr const char* kFontCandidates[] = {
			"C:\\Windows\\Fonts\\msyh.ttc",
			"C:\\Windows\\Fonts\\msyhl.ttc",
			"C:\\Windows\\Fonts\\simhei.ttf",
		};
		constexpr float kFontSize   = 17.0f;
		constexpr float kPanelWidth = 360.0f; // logical pixels the controls column takes from the window

		// The controls column follows the playground's light chrome, with a soft divider against the lyric area.
		constexpr SkColor kControlBackground = SkColorSetRGB(0xF8, 0xF8, 0xF8); // playground --color-bg-alt
		constexpr SkColor kDividerColor      = SkColorSetRGB(0xD4, 0xD4, 0xD4); // playground --color-border-strong

		/**
		 * Formats a millisecond position as `m:ss`.
		 */
		void formatTime(char* buffer, std::size_t size, double milliseconds) {
			const int total   = milliseconds > 0.0 ? static_cast<int>(milliseconds / 1000.0) : 0;
			const int minutes = total / 60;
			const int seconds = total % 60;
			std::snprintf(buffer, size, "%d:%02d", minutes, seconds);
		}

		/**
		 * Loads the first available CJK-capable system font at `scale`, falling back to ImGui's built-in one.
		 */
		void loadFont(ImGuiIO& io, float scale) {
			for (const char* path : kFontCandidates) {
				std::error_code error;
				if (std::filesystem::exists(path, error)) {
					// ImGui 1.92 rasterises glyphs on demand, so no range table is needed up front.
					if (io.Fonts->AddFontFromFileTTF(path, kFontSize * scale) != nullptr) {
						return;
					}
				}
			}
			io.Fonts->AddFontDefault();
		}
	} // namespace

	ControlPanel::ControlPanel() = default;

	ControlPanel::~ControlPanel() {
		if (!this->ready) {
			return;
		}

		ImGui::SetCurrentContext(this->context);
		shutdownImGuiSkia();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext(this->context);
		this->context = nullptr;
	}

	bool ControlPanel::init(GLFWwindow* window, float devicePixelRatio) {
		this->scale = devicePixelRatio > 0.0f ? devicePixelRatio : 1.0f;

		IMGUI_CHECKVERSION();
		this->context = ImGui::CreateContext();
		ImGui::SetCurrentContext(this->context);

		ImGuiIO& io = ImGui::GetIO();
		// The demo keeps its own state file, so ImGui should not drop an ini next to the executable.
		io.IniFilename = nullptr;
		// GLFW reports this window in physical pixels, so the whole panel is scaled up instead of shrinking on a high-DPI monitor.
		loadFont(io, this->scale);
		// The playground is a light theme, so the controls match rather than sitting dark beside a white lyric area.
		ImGui::StyleColorsLight();
		ImGui::GetStyle().ScaleAllSizes(this->scale);

		// ImGui chains onto the callbacks the window already installed, so the demo's key bindings keep working.
		if (!ImGui_ImplGlfw_InitForOther(window, true)) {
			std::fprintf(stderr, "[example] failed to attach imgui to the window\n");
			ImGui::DestroyContext(this->context);
			this->context = nullptr;
			return false;
		}
		initImGuiSkia();

		this->ready = true;
		return true;
	}

	int ControlPanel::width() const {
		return visible() ? static_cast<int>(kPanelWidth * this->scale) : 0;
	}

	bool ControlPanel::visible() const {
		return this->ready && this->shown;
	}

	void ControlPanel::setVisible(bool visible) {
		this->shown = visible;
	}

	bool ControlPanel::capturesKeyboard() const {
		return this->ready && ImGui::GetIO().WantCaptureKeyboard;
	}

	PanelActions ControlPanel::render(SkCanvas* canvas, const PanelState& state, int height) {
		PanelActions actions;
		if (!this->ready) {
			return actions;
		}

		const float columnWidth  = static_cast<float>(width());
		const float columnHeight = static_cast<float>(height);

		// The frame runs even while the panel is hidden: only NewFrame drains the input events ImGui's callbacks queue up.
		ImGui::SetCurrentContext(this->context);
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (this->shown) {
			// One panel pinned to the left column: the host window is the frame, so ImGui draws no second border inside it.
			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(columnWidth, columnHeight));
			// The column background is painted below through Skia, so the ImGui window itself stays transparent.
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::Begin("controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

			if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::TextUnformatted("Audio");
				ImGui::TextWrapped("%s", state.hasAudio ? state.trackName.c_str() : "(none)");
				if (ImGui::Button("Open audio...", ImVec2(-1.0f, 0.0f))) {
					actions.openAudio = true;
				}

				ImGui::Spacing();
				ImGui::TextUnformatted("Lyric");
				ImGui::TextWrapped("%s", state.hasLyric ? state.lyricName.c_str() : "(none)");
				if (ImGui::Button("Load lyric hex...", ImVec2(-1.0f, 0.0f))) {
					actions.loadLyric = true;
				}
			}

			if (ImGui::CollapsingHeader("Playback", ImGuiTreeNodeFlags_DefaultOpen)) {
				if (ImGui::Button(state.playing ? "Pause" : "Play", ImVec2(88.0f * this->scale, 0.0f))) {
					actions.togglePause = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("Restart", ImVec2(88.0f * this->scale, 0.0f))) {
					actions.restart = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("-5s", ImVec2(52.0f * this->scale, 0.0f))) {
					actions.seek = state.positionMs - 5000.0;
				}
				ImGui::SameLine();
				if (ImGui::Button("+5s", ImVec2(52.0f * this->scale, 0.0f))) {
					actions.seek = state.positionMs + 5000.0;
				}

				char position[16] = {};
				char total[16]    = {};
				formatTime(position, sizeof(position), state.positionMs);
				formatTime(total, sizeof(total), state.durationMs);

				if (state.durationMs > 0.0) {
					// Dragging reports every frame, so the host seeks continuously while the handle moves.
					float       seconds = static_cast<float>(state.positionMs / 1000.0);
					const float length  = static_cast<float>(state.durationMs / 1000.0);
					ImGui::SetNextItemWidth(-1.0f);
					if (ImGui::SliderFloat("##position", &seconds, 0.0f, length, "")) {
						actions.seek = static_cast<double>(seconds) * 1000.0;
					}
					ImGui::Text("%s / %s", position, total);
				} else {
					// Without a loaded track there is nothing to seek against, so the bar is inert and the total is unknown.
					float dummy = 0.0f;
					ImGui::BeginDisabled();
					ImGui::SetNextItemWidth(-1.0f);
					ImGui::SliderFloat("##position", &dummy, 0.0f, 1.0f, "");
					ImGui::EndDisabled();
					ImGui::Text("%s / --:--", position);
				}

				float volume = state.volume;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat("##volume", &volume, 0.0f, 1.0f, "volume %.2f")) {
					actions.volume = volume;
				}
				// Persist only when the drag ends, so the host is not rewriting the state file every frame.
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					actions.volumeCommitted = true;
				}
			}

			if (ImGui::CollapsingHeader("Status", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("State: %s", state.playing ? "playing" : "paused");
				if (state.activeLine >= 0) {
					ImGui::Text("Line: %d / %d", state.activeLine + 1, state.lineCount);
				} else {
					ImGui::Text("Line: - / %d", state.lineCount);
				}
				ImGui::Spacing();
				ImGui::TextUnformatted("Current line");
				ImGui::TextWrapped("%s", state.activeText.empty() ? "-" : state.activeText.c_str());
			}

			// The editor is last: it is by far the tallest section, so the transport controls stay reachable above it.
			if (state.config != nullptr && state.overrides != nullptr && ImGui::CollapsingHeader("Settings")) {
				const SettingsEdit edit   = drawSettings(*state.config, *state.overrides);
				actions.settingsChanged   = edit.changed;
				actions.settingsCommitted = edit.committed;
				actions.settingsReset     = edit.reset;
			}

			ImGui::End();
		}

		ImGui::Render();

		if (this->shown) {
			// The column background sits under the transparent ImGui window, so the controls read on the playground's light chrome.
			SkPaint background;
			background.setColor(kControlBackground);
			canvas->drawRect(SkRect::MakeWH(columnWidth, columnHeight), background);
		}
		// Always drawn: even an empty frame carries the texture requests ImGui expects the backend to service.
		renderImGuiSkia(canvas, ImGui::GetDrawData());

		if (this->shown) {
			// A one-pixel rule down the right edge separates the controls from the lyric area.
			const float  lineWidth = std::max(1.0f, this->scale);
			SkPaint      divider;
			divider.setColor(kDividerColor);
			canvas->drawRect(SkRect::MakeXYWH(columnWidth - lineWidth, 0.0f, lineWidth, columnHeight), divider);
		}

		return actions;
	}
} // namespace example
