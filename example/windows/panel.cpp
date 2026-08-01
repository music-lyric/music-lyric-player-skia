#include "panel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "imgui_skia.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"
#include "settings.h"
#include "theme.h"
#include "utils/logger/logger.h"
#include "widgets.h"

#ifndef MUSIC_LYRIC_PLAYER_VERSION
#define MUSIC_LYRIC_PLAYER_VERSION "dev"
#endif

namespace example {
	namespace {
		constexpr music_lyric_player::utils::Logger logger{"ControlPanel"};

		namespace color = theme::color;

		constexpr const char* kTabs[] = {"Audio", "Lyric", "Settings"};

		/**
		 * Converts a packed ImGui colour into the ARGB layout Skia's paints take.
		 */
		SkColor toSkColor(ImU32 value) {
			return SkColorSetARGB(static_cast<std::uint8_t>((value >> IM_COL32_A_SHIFT) & 0xFF),
				static_cast<std::uint8_t>((value >> IM_COL32_R_SHIFT) & 0xFF),
				static_cast<std::uint8_t>((value >> IM_COL32_G_SHIFT) & 0xFF),
				static_cast<std::uint8_t>((value >> IM_COL32_B_SHIFT) & 0xFF));
		}

		/**
		 * Fills a rectangle of the frame with one of the palette's colours.
		 */
		void fill(SkCanvas* canvas, float x, float y, float width, float height, ImU32 value) {
			SkPaint paint;
			paint.setColor(toSkColor(value));
			canvas->drawRect(SkRect::MakeXYWH(x, y, width, height), paint);
		}

		/**
		 * Formats a millisecond position as `m:ss`, or as a pair of dashes when there is no track to measure against.
		 */
		void formatTime(char* buffer, std::size_t size, double milliseconds, bool known) {
			if (!known) {
				std::snprintf(buffer, size, "--:--");
				return;
			}

			const int total   = milliseconds > 0.0 ? static_cast<int>(milliseconds / 1000.0) : 0;
			const int minutes = total / 60;
			const int seconds = total % 60;
			std::snprintf(buffer, size, "%d:%02d", minutes, seconds);
		}

		/**
		 * Adds vertical room between two runs of a tab, the way the stylesheet's gaps do.
		 */
		void space(float amount) {
			ImGui::Dummy(ImVec2(0.0f, amount));
		}

		/**
		 * Draws a read-only row of the status card: a label on the left and its value in the control column.
		 */
		void statusRow(const char* label, const char* value) {
			widgets::field(label);
			ImGui::PushFont(nullptr, theme::metrics().fontSmall);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::text));
			ImGui::TextUnformatted(value);
			ImGui::PopStyleColor();
			ImGui::PopFont();
		}

		/**
		 * Draws the sidebar's title block: the product name with its version, and the one line subtitle under it.
		 */
		void drawHeader() {
			const theme::Metrics& metrics = theme::metrics();

			widgets::beginInset(20.0f * metrics.scale, 20.0f * metrics.scale);
			space(12.0f * metrics.scale);

			ImGui::PushFont(theme::semibold(), metrics.fontTitle);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::text));
			ImGui::TextUnformatted("music-lyric-player");
			ImGui::PopStyleColor();
			ImGui::PopFont();

			ImGui::SameLine(0.0f, 4.0f * metrics.scale);
			ImGui::PushFont(nullptr, metrics.fontTiny);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::textMuted));
			ImGui::Text("(v%s)", MUSIC_LYRIC_PLAYER_VERSION);
			ImGui::PopStyleColor();
			ImGui::PopFont();

			ImGui::PushFont(nullptr, metrics.fontSmall);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::textMuted));
			ImGui::TextUnformatted("Skia renderer playground");
			ImGui::PopStyleColor();
			ImGui::PopFont();

			space(8.0f * metrics.scale);
			widgets::endInset();
			widgets::rule();
		}

		/**
		 * Draws the audio tab: one card that opens the file picker and reports what is loaded.
		 */
		void drawAudioTab(const PanelState& state, PanelActions& actions) {
			const theme::Metrics& metrics = theme::metrics();

			widgets::beginInset(metrics.framePadding, metrics.framePadding);
			space(metrics.framePadding);
			widgets::heading("Audio", "mp3 / wav / flac");
			space(4.0f * metrics.scale);
			if (widgets::picker("##audio",
				    widgets::Icon::Music,
				    state.hasAudio ? "Replace the track" : "Choose an audio file",
				    state.hasAudio ? state.trackName.c_str() : "No file loaded",
				    state.hasAudio)) {
				actions.openAudio = true;
			}
			widgets::endInset();
		}

		/**
		 * Draws the lyric tab: the hex loader, then what the player currently makes of it.
		 */
		void drawLyricTab(const PanelState& state, PanelActions& actions) {
			const theme::Metrics& metrics = theme::metrics();

			widgets::beginInset(metrics.framePadding, metrics.framePadding);
			space(metrics.framePadding);
			widgets::heading("Lyric", "protobuf hex");
			space(4.0f * metrics.scale);
			if (widgets::picker("##lyric",
				    widgets::Icon::Lyric,
				    state.hasLyric ? "Replace the lyric" : "Load a lyric",
				    state.hasLyric ? state.lyricName.c_str() : "No lyric loaded",
				    state.hasLyric)) {
				actions.loadLyric = true;
			}

			space(16.0f * metrics.scale);
			widgets::heading("Status");
			space(4.0f * metrics.scale);

			widgets::beginCard("##status");
			statusRow("State", state.playing ? "Playing" : "Paused");

			char position[24] = {};
			if (state.activeLine >= 0) {
				std::snprintf(position, sizeof(position), "%d / %d", state.activeLine + 1, state.lineCount);
			} else {
				std::snprintf(position, sizeof(position), "- / %d", state.lineCount);
			}
			statusRow("Line", position);

			space(4.0f * metrics.scale);
			widgets::groupTitle("Current line");
			widgets::body(state.activeText.empty() ? "-" : state.activeText.c_str(), state.activeText.empty());
			widgets::endCard();

			widgets::endInset();
		}

		/**
		 * Draws the settings tab: the scope header with its reset, then the accordion of every editable leaf.
		 */
		void drawSettingsTab(const PanelState& state, PanelActions& actions) {
			const theme::Metrics& metrics = theme::metrics();
			if (state.config == nullptr || state.overrides == nullptr) {
				return;
			}

			widgets::beginInset(metrics.framePadding, metrics.framePadding);
			space(metrics.framePadding);

			widgets::heading("Rendering");
			ImGui::SameLine();
			// The reset sits at the right end of the header row, the way the playground's scope header carries it.
			const float reset = widgets::pillButtonWidth("Reset");
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - metrics.framePadding - reset));
			if (widgets::pillButton("##reset", widgets::Icon::Restart, "Reset")) {
				actions.settingsReset = true;
			}
			space(4.0f * metrics.scale);

			widgets::beginSurface("##sections", ImGui::GetContentRegionAvail().y - metrics.framePadding);
			const SettingsEdit edit   = drawSettings(*state.config, *state.overrides);
			actions.settingsChanged   = edit.changed;
			actions.settingsCommitted = edit.committed;
			widgets::endSurface();

			widgets::endInset();
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
		// GLFW reports this window in physical pixels, so every length is scaled up rather than the panel shrinking on a high-DPI monitor.
		// The theme folds that scale into its metrics.
		theme::apply(this->scale);

		// ImGui chains onto the callbacks the window already installed, so the demo's key bindings keep working.
		if (!ImGui_ImplGlfw_InitForOther(window, true)) {
			logger.error("failed to attach imgui to the window");
			ImGui::DestroyContext(this->context);
			this->context = nullptr;
			return false;
		}
		initImGuiSkia();

		this->ready = true;
		return true;
	}

	int ControlPanel::width() const {
		return this->ready ? static_cast<int>(theme::metrics().sidebarWidth * this->reveal) : 0;
	}

	int ControlPanel::controlsHeight() const {
		return this->ready ? static_cast<int>(theme::metrics().controlsHeight) : 0;
	}

	PanelActions ControlPanel::render(SkCanvas* canvas, const PanelState& state, int width, int height) {
		PanelActions actions;
		if (!this->ready) {
			return actions;
		}

		const theme::Metrics& metrics     = theme::metrics();
		const float           frameWidth  = static_cast<float>(width);
		const float           frameHeight = static_cast<float>(height);
		const float           sidebar     = static_cast<float>(this->width());
		const float           barHeight   = metrics.controlsHeight;
		const float           barTop      = frameHeight - barHeight;
		const float           hairline    = std::max(1.0f, metrics.scale);

		// The frame runs even while the sidebar is hidden: only NewFrame drains the input events ImGui's callbacks queue up.
		ImGui::SetCurrentContext(this->context);
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// A click on the menu button only lands next frame, so the width the host already clipped against stays honest.
		bool nextShown = this->shown;

		if (sidebar > 0.0f) {
			// One window pinned to the left column: the host window is the frame, so ImGui draws no second border inside it.
			// It keeps its full width through the slide and hangs off the left edge instead, so nothing inside reflows.
			ImGui::SetNextWindowPos(ImVec2(sidebar - metrics.sidebarWidth, 0.0f));
			ImGui::SetNextWindowSize(ImVec2(metrics.sidebarWidth, frameHeight));
			// The column background is painted below through Skia, so the ImGui window itself stays transparent.
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("sidebar",
				nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

			drawHeader();

			widgets::beginInset(metrics.framePadding, metrics.framePadding);
			space(12.0f * metrics.scale);
			widgets::segmented("##tabs", kTabs, IM_ARRAYSIZE(kTabs), &this->tab);
			space(4.0f * metrics.scale);
			widgets::endInset();

			// Each tab scrolls on its own, so switching back to one lands where it was left.
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::BeginChild("##body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
			switch (this->tab) {
			case 0:
				drawAudioTab(state, actions);
				break;
			case 1:
				drawLyricTab(state, actions);
				break;
			default:
				drawSettingsTab(state, actions);
				break;
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();

			ImGui::End();
			ImGui::PopStyleVar();
		}

		// The transport spans whatever the sidebar leaves, and stays put while the sidebar is hidden.
		ImGui::SetNextWindowPos(ImVec2(sidebar, barTop));
		ImGui::SetNextWindowSize(ImVec2(frameWidth - sidebar, barHeight));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("transport",
			nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
		{
			char position[16] = {};
			char total[16]    = {};
			formatTime(position, sizeof(position), state.positionMs, true);
			formatTime(total, sizeof(total), state.durationMs, state.durationMs > 0.0);
			char elapsed[40] = {};
			std::snprintf(elapsed, sizeof(elapsed), "%s / %s", position, total);

			char timing[48] = {};
			std::snprintf(timing, sizeof(timing), "%.0fFPS - %.2fms / %.2fms", state.timing.fps, state.timing.drawMs, state.timing.totalMs);

			ImGui::PushFont(nullptr, metrics.fontSmall);
			const float timeWidth   = ImGui::CalcTextSize(elapsed).x;
			const float timingWidth = ImGui::CalcTextSize(timing).x;
			// The slot is measured from an all-digit template rather than from the text itself, so a frame rate crossing a digit boundary cannot twitch the seek track.
			const float readoutWidth = ImGui::CalcTextSize("000FPS - 00.00ms / 00.00ms").x;
			ImGui::PopFont();

			const float gap     = 12.0f * metrics.scale;
			const float padding = metrics.framePadding;
			const float fixed   = padding * 2.0f + metrics.iconButton * 3.0f + metrics.playButton + timeWidth + metrics.volumeWidth + readoutWidth + gap * 6.0f;
			const float track   = std::max(40.0f * metrics.scale, frameWidth - sidebar - fixed);

			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const float  middle = origin.y + barHeight * 0.5f;
			float        cursor = origin.x + padding;

			// Every control is centred on the bar, so each one is placed rather than laid out in a row.
			const auto place = [&](float itemWidth, float itemHeight) {
				ImGui::SetCursorScreenPos(ImVec2(cursor, middle - itemHeight * 0.5f));
				cursor += itemWidth + gap;
			};

			place(metrics.iconButton, metrics.iconButton);
			if (widgets::iconButton("##menu", widgets::Icon::Menu, metrics.iconButton, this->shown)) {
				nextShown = !nextShown;
			}

			place(metrics.iconButton, metrics.iconButton);
			if (widgets::iconButton("##restart", widgets::Icon::Restart, metrics.iconButton, false, !state.hasLyric && !state.hasAudio)) {
				actions.restart = true;
			}

			place(metrics.playButton, metrics.playButton);
			if (widgets::playButton("##play", state.playing, !state.hasAudio && !state.hasLyric)) {
				actions.togglePause = true;
			}

			place(track, metrics.sliderHeight);
			const widgets::Drag seek = widgets::slider("##seek",
				track,
				static_cast<float>(state.positionMs),
				0.0f,
				static_cast<float>(std::max(state.durationMs, 1.0)),
				state.durationMs <= 0.0);
			if (seek.changed) {
				actions.seek = static_cast<double>(seek.value);
			}

			place(timeWidth, metrics.fontSmall);
			ImGui::PushFont(nullptr, metrics.fontSmall);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::textSecondary));
			ImGui::TextUnformatted(elapsed);
			ImGui::PopStyleColor();
			ImGui::PopFont();

			const bool muted = state.volume <= 0.0f;
			place(metrics.iconButton, metrics.iconButton);
			if (widgets::iconButton("##mute", muted ? widgets::Icon::VolumeMuted : widgets::Icon::Volume, metrics.iconButton)) {
				actions.volume          = muted ? this->mutedVolume : 0.0f;
				actions.volumeCommitted = true;
				if (!muted) {
					this->mutedVolume = state.volume;
				}
			}

			place(metrics.volumeWidth, metrics.sliderHeight);
			const widgets::Drag level = widgets::slider("##volume", metrics.volumeWidth, state.volume, 0.0f, 1.0f, false);
			if (level.changed) {
				actions.volume = level.value;
				if (level.value > 0.0f) {
					this->mutedVolume = level.value;
				}
			}
			// Persist only when the drag ends, so the host is not rewriting the state file every frame.
			if (level.released) {
				actions.volumeCommitted = true;
			}

			// The readout closes the bar, so it is right-aligned inside its reserved slot instead of leaving a ragged edge as the digits change.
			place(readoutWidth, metrics.fontSmall);
			const ImVec2 readout = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos(ImVec2(readout.x + readoutWidth - timingWidth, readout.y));
			ImGui::PushFont(nullptr, metrics.fontSmall);
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(color::textMuted));
			ImGui::TextUnformatted(timing);
			ImGui::PopStyleColor();
			ImGui::PopFont();
			// The total says how long the surface was busy but not where it waited, which is the one thing worth knowing: a vsync bound loop should be waiting in acquire, and waiting in present instead means the GPU is behind.
			ImGui::SetItemTooltip("acquire %.2fms\ndraw %.2fms\npresent %.2fms", state.timing.acquireMs, state.timing.drawMs, state.timing.presentMs);
		}
		ImGui::End();
		ImGui::PopStyleVar();

		ImGui::Render();

		// The chrome sits under the transparent ImGui windows, so the controls read on the playground's light frame.
		if (sidebar > 0.0f) {
			fill(canvas, 0.0f, 0.0f, sidebar, frameHeight, color::background);
		}
		fill(canvas, sidebar, barTop, frameWidth - sidebar, barHeight, color::background);
		fill(canvas, sidebar, barTop, frameWidth - sidebar, hairline, color::border);
		if (sidebar > 0.0f) {
			// A hairline down the right edge separates the sidebar from the lyric area.
			fill(canvas, sidebar - hairline, 0.0f, hairline, frameHeight, color::border);
		}

		// Always drawn: even an empty frame carries the texture requests ImGui expects the backend to service.
		renderImGuiSkia(canvas, ImGui::GetDrawData());

		this->shown = nextShown;
		// Eased after the frame is built, so the width the host clipped against is the one the panel just drew at.
		// The approach only nears its target, so the last half pixel is snapped away and the sidebar settles exactly.
		const float target = this->shown ? 1.0f : 0.0f;
		this->reveal       = widgets::approach(this->reveal, target, theme::motion::base);
		if (std::abs(target - this->reveal) * metrics.sidebarWidth < 0.5f) {
			this->reveal = target;
		}
		return actions;
	}
} // namespace example
