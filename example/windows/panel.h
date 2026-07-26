#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_PANEL_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_PANEL_H_

#include <optional>
#include <string>

class SkCanvas;
struct GLFWwindow;
struct ImGuiContext;

namespace example {
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
	};

	/**
	 * The demo's controls, drawn with ImGui into the left column of the lyric window.
	 * It owns no window and no GPU state: the host reserves the column, draws the lyrics beside it,
	 * and hands the same canvas over for the panel to paint itself onto.
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
		 * Builds one frame and paints it into the left `width()` pixels of `canvas`, spanning `height` pixels.
		 * Returns what the user asked for while the frame was built; the host applies it after the frame.
		 */
		PanelActions render(SkCanvas* canvas, const PanelState& state, int height);

		/**
		 * Returns the width in physical pixels the panel reserves on the left, or zero while it is hidden.
		 */
		int width() const;

		/**
		 * Reports whether the panel column is currently shown.
		 */
		bool visible() const;

		/**
		 * Shows or hides the panel column; the lyrics take the whole surface while it is hidden.
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
	};
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_PANEL_H_
