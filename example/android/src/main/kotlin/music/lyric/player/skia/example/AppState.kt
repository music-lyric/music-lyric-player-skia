package music.lyric.player.skia.example

import android.net.Uri
import java.io.File
import music.lyric.player.skia.config.LayoutAlign
import org.json.JSONObject

class AppState(directory: File) {
    data class Settings(
        val fontSize: Int = 50,
        val lineGap: Int = 45,
        val align: LayoutAlign = LayoutAlign.Left,
        val enableSyllable: Boolean = true,
    )

    private val file = File(directory, "state.json")

    private val lyricFile = File(directory, "lyric.bin")

    var audio: Uri? = null

    var settings = Settings()

    fun load() {
        val json = try {
            JSONObject(this.file.readText())
        } catch (error: Exception) {
            return
        }

        this.audio = json.optString(KEY_AUDIO).takeIf { it.isNotEmpty() }?.let(Uri::parse)

        val stored = json.optJSONObject(KEY_SETTINGS) ?: return
        val defaults = Settings()
        this.settings = Settings(
            fontSize = stored.optInt(KEY_FONT_SIZE, defaults.fontSize),
            lineGap = stored.optInt(KEY_LINE_GAP, defaults.lineGap),
            align = LayoutAlign.entries.firstOrNull { it.wire == stored.optString(KEY_ALIGN) } ?: defaults.align,
            enableSyllable = stored.optBoolean(KEY_ENABLE_SYLLABLE, defaults.enableSyllable),
        )
    }

    fun save() {
        val stored = JSONObject()
            .put(KEY_FONT_SIZE, this.settings.fontSize)
            .put(KEY_LINE_GAP, this.settings.lineGap)
            .put(KEY_ALIGN, this.settings.align.wire)
            .put(KEY_ENABLE_SYLLABLE, this.settings.enableSyllable)

        val json = JSONObject()
            .put(KEY_AUDIO, this.audio?.toString().orEmpty())
            .put(KEY_SETTINGS, stored)

        try {
            this.file.writeText(json.toString())
        } catch (error: Exception) {
            // A session that cannot be written is worth no more than the launch it belongs to.
        }
    }

    fun readLyric(): ByteArray? {
        if (!this.lyricFile.isFile) {
            return null
        }

        return try {
            this.lyricFile.readBytes()
        } catch (error: Exception) {
            null
        }
    }

    fun writeLyric(bytes: ByteArray) {
        try {
            this.lyricFile.writeBytes(bytes)
        } catch (error: Exception) {
            // As with the session itself, a lyric that cannot be written only costs the next launch its head start.
        }
    }

    companion object {
        private const val KEY_AUDIO = "audio"
        private const val KEY_SETTINGS = "settings"

        private const val KEY_FONT_SIZE = "fontSize"
        private const val KEY_LINE_GAP = "lineGap"
        private const val KEY_ALIGN = "align"
        private const val KEY_ENABLE_SYLLABLE = "enableSyllable"
    }
}
