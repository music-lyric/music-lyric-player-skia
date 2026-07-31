#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_PANEL_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_PANEL_H_

#include <optional>
#include <string>

#include "rendering/config/config.h"

class SkCanvas;
struct GLFWwindow;
struct ImGuiContext;

namespace example {
	/**
	 * How long one frame took, split into the three spans the host can time from outside the GPU surface.
	 * A vsync bound loop should spend nearly all of its wait in `acquireMs`: waiting there is waiting for the display, while waiting in `presentMs` is waiting for the GPU to catch up.
	 */
	struct FrameTiming {
		double fps = 0.0;
		// The whole span the surface was busy, which is the three below added up.
		double totalMs = 0.0;
		// Waiting for a free backbuffer.
		double acquireMs = 0.0;
		// Recording the lyrics and the panel.
		double drawMs = 0.0;
		// Flushing, submitting and presenting.
		double presentMs = 0.0;
	};

	/**
	 * What the host tells the panel to display for the current frame.
	 */
	struct PanelState {
		bool        playing    = false;
		double      positionMs = 0.0;
		double      durationMs = 0.0;
		float       volume     = 1.0f;
		bool        hasAudio   = false;
		bool        hasLyric   = false;
		int         activeLine = -1;
		int         lineCount  = 0;
		std::string trackName;
		std::string lyricName;
		std::string activeText;
		// Describes the frame before this one: the panel that displays it is painted inside the span it measures.
		FrameTiming timing;
		// The resolved config the settings editor displays, and the sparse override store its edits accumulate into.
		const music_lyric_player::rendering::config::Root* config    = nullptr;
		music_lyric_player::rendering::config::Root*       overrides = nullptr;
	};

	/**
	 * What the panel asks the host to do once the frame has been built.
	 */
	struct PanelActions {
		bool                  togglePause = false;
		bool                  restart     = false;
		bool                  openAudio   = false;
		bool                  loadLyric   = false;
		std::optional<double> seek;
		std::optional<float>  volume;
		// True on the frame the volume drag ends, so the host persists the level without writing every frame.
		bool volumeCommitted = false;
		// A settings field changed a leaf, so the host merges the override store into the renderer.
		bool settingsChanged = false;
		// A settings edit finished, so the host mirrors the override store to disk.
		bool settingsCommitted = false;
		// The user dropped every override, so the host clears the store and restores its own defaults.
		bool settingsReset = false;
	};

	/**
	 * The demo's controls, drawn with ImGui into the lyric window and styled after the web playground.
	 * They occupy two regions the host reserves: a sidebar down the left, and a transport bar along the bottom of whatever the sidebar leaves.
	 * The panel owns no window and no GPU state, so the host hands the same canvas over for it to paint itself onto.
	 */
	class ControlPanel {
	public:
		ControlPanel();
		~ControlPanel();

		ControlPanel(const ControlPanel&)            = delete;
		ControlPanel& operator=(const ControlPanel&) = delete;

		/**
		 * Creates the ImGui context, attaches it to `window` for input and scales it for `devicePixelRatio`.
		 * Returns false when ImGui cannot be attached, in which case the panel stays hidden.
		 */
		bool init(GLFWwindow* window, float devicePixelRatio);

		/**
		 * Builds one frame and paints it into the `width` by `height` pixel surface `canvas` covers.
		 * Returns what the user asked for while the frame was built; the host applies it after the frame.
		 */
		PanelActions render(SkCanvas* canvas, const PanelState& state, int width, int height);

		/**
		 * Returns the width in physical pixels the sidebar reserves on the left, which follows it in and out while it slides and reaches zero once it is hidden.
		 */
		int width() const;

		/**
		 * Returns the height in physical pixels the transport bar reserves along the bottom.
		 */
		int controlsHeight() const;

		/**
		 * Reports whether the sidebar is currently shown.
		 */
		bool visible() const;

		/**
		 * Shows or hides the sidebar; the lyrics take its width back while it is hidden.
		 */
		void setVisible(bool visible);

		/**
		 * Reports whether ImGui consumed the keyboard on the frame just built, so the host can skip its shortcuts.
		 */
		bool capturesKeyboard() const;

	private:
		ImGuiContext* context = nullptr;
		bool          ready   = false;
		bool          shown   = true;
		float         scale   = 1.0f;
		// How much of the sidebar is currently out, from fully collapsed at zero to fully open at one.
		// It eases toward `shown` once per frame, so the sidebar slides in and out instead of snapping.
		float reveal = 1.0f;
		// Which of the sidebar's three tabs is open.
		int tab = 0;
		// The level to restore when the speaker is clicked again, since muting is stored as a volume of zero.
		float mutedVolume = 1.0f;
	};
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_PANEL_H_
