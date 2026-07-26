#include "imgui_skia.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "imgui.h"
#include "include/core/SkAlphaType.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorType.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkVertices.h"

namespace example {
	namespace {
		/**
		 * Returns the Skia image an ImGui texture identifier refers to, or null when unset.
		 */
		SkImage* asImage(ImTextureID id) {
			return id == ImTextureID_Invalid ? nullptr : reinterpret_cast<SkImage*>(static_cast<std::intptr_t>(id));
		}

		/**
		 * Converts an ImGui packed colour, whose byte order is configurable, to Skia's ARGB layout.
		 */
		SkColor toSkColor(ImU32 color) {
			return SkColorSetARGB(static_cast<std::uint8_t>((color >> IM_COL32_A_SHIFT) & 0xFF),
				static_cast<std::uint8_t>((color >> IM_COL32_R_SHIFT) & 0xFF),
				static_cast<std::uint8_t>((color >> IM_COL32_G_SHIFT) & 0xFF),
				static_cast<std::uint8_t>((color >> IM_COL32_B_SHIFT) & 0xFF));
		}

		/**
		 * Copies an ImGui texture's pixels into an immutable Skia image, or returns null for a format this backend does not take.
		 * The atlas stores white texels carrying a straight alpha, so it is tagged unpremultiplied rather than premultiplied.
		 */
		sk_sp<SkImage> makeAtlasImage(ImTextureData* texture) {
			if (texture->Format != ImTextureFormat_RGBA32) {
				// Every draw command would silently skip in this case, so report it rather than render nothing.
				std::fprintf(stderr, "[example] imgui asked for an unsupported texture format %d\n", static_cast<int>(texture->Format));
				return nullptr;
			}

			const SkImageInfo info = SkImageInfo::Make(texture->Width, texture->Height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
			const SkPixmap    pixmap(info, texture->GetPixels(), static_cast<std::size_t>(texture->GetPitch()));
			sk_sp<SkImage>    image = SkImages::RasterFromPixmapCopy(pixmap);
			if (!image) {
				std::fprintf(stderr, "[example] failed to copy a %dx%d imgui atlas into a skia image\n", texture->Width, texture->Height);
			}
			return image;
		}

		/**
		 * Applies one pending create, update or destroy request from ImGui's texture list.
		 */
		void updateTexture(ImTextureData* texture) {
			switch (texture->Status) {
			case ImTextureStatus_WantCreate:
			case ImTextureStatus_WantUpdates: {
				// A Skia image is immutable, so a partial update rebuilds the whole atlas; ImGui only asks for one when new glyphs appear.
				sk_sp<SkImage> image = makeAtlasImage(texture);
				if (!image) {
					return;
				}
				SkSafeUnref(asImage(texture->GetTexID()));
				texture->SetTexID(static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(image.release())));
				texture->SetStatus(ImTextureStatus_OK);
				break;
			}
			case ImTextureStatus_WantDestroy:
				SkSafeUnref(asImage(texture->GetTexID()));
				texture->SetTexID(ImTextureID_Invalid);
				texture->SetStatus(ImTextureStatus_Destroyed);
				break;
			default:
				break;
			}
		}

		/**
		 * Paints one draw command's triangles, mapping ImGui's normalised texture coordinates onto the atlas.
		 */
		void drawCommand(SkCanvas* canvas, const ImDrawList* list, const ImDrawCmd& command, const ImVec2& origin) {
			SkImage* image = asImage(command.GetTexID());
			if (image == nullptr || command.ElemCount == 0) {
				return;
			}

			// Skia indexes into a flat vertex array, so the command's vertex window is converted whole.
			const ImDrawVert* source = list->VtxBuffer.Data + command.VtxOffset;
			const int         count  = list->VtxBuffer.Size - static_cast<int>(command.VtxOffset);
			if (count <= 0) {
				return;
			}

			std::vector<SkPoint> positions(static_cast<std::size_t>(count));
			std::vector<SkPoint> texCoords(static_cast<std::size_t>(count));
			std::vector<SkColor> colors(static_cast<std::size_t>(count));
			const float          textureWidth  = static_cast<float>(image->width());
			const float          textureHeight = static_cast<float>(image->height());
			for (int i = 0; i < count; ++i) {
				const ImDrawVert& vertex               = source[i];
				positions[static_cast<std::size_t>(i)] = SkPoint::Make(vertex.pos.x - origin.x, vertex.pos.y - origin.y);
				// Skia samples an image shader in texel space, so the normalised coordinates are scaled up here.
				texCoords[static_cast<std::size_t>(i)] = SkPoint::Make(vertex.uv.x * textureWidth, vertex.uv.y * textureHeight);
				colors[static_cast<std::size_t>(i)]    = toSkColor(vertex.col);
			}

			const sk_sp<SkVertices> vertices = SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode,
				count,
				positions.data(),
				texCoords.data(),
				colors.data(),
				static_cast<int>(command.ElemCount),
				list->IdxBuffer.Data + command.IdxOffset);
			if (!vertices) {
				return;
			}

			SkPaint paint;
			paint.setShader(image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, SkSamplingOptions(SkFilterMode::kLinear)));

			canvas->save();
			canvas->clipRect(
				SkRect::MakeLTRB(command.ClipRect.x - origin.x, command.ClipRect.y - origin.y, command.ClipRect.z - origin.x, command.ClipRect.w - origin.y));
			// The vertex colour modulates the atlas texel, which is how ImGui tints glyphs and shapes.
			canvas->drawVertices(vertices, SkBlendMode::kModulate, paint);
			canvas->restore();
		}
	} // namespace

	void initImGuiSkia() {
		ImGuiIO& io            = ImGui::GetIO();
		io.BackendRendererName = "imgui_impl_skia";
		// The backend owns texture lifetimes and can index into a shared vertex buffer, so ImGui need not split draw lists.
		io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		// An alpha-only atlas would take its colour from the paint rather than the per-vertex tint, so ask for full RGBA texels.
		io.Fonts->TexDesiredFormat = ImTextureFormat_RGBA32;
	}

	void shutdownImGuiSkia() {
		// Destroying the textures here also clears the identifiers ImGui would otherwise hand back after shutdown.
		for (ImTextureData* texture : ImGui::GetPlatformIO().Textures) {
			SkSafeUnref(asImage(texture->GetTexID()));
			texture->SetTexID(ImTextureID_Invalid);
			texture->SetStatus(ImTextureStatus_Destroyed);
		}

		ImGuiIO& io            = ImGui::GetIO();
		io.BackendRendererName = nullptr;
		io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasTextures | ImGuiBackendFlags_RendererHasVtxOffset);
	}

	void renderImGuiSkia(SkCanvas* canvas, ImDrawData* drawData) {
		if (canvas == nullptr || drawData == nullptr) {
			return;
		}

		// ImGui may create or grow the atlas mid-frame, so pending texture requests are serviced before the draw lists reference them.
		if (drawData->Textures != nullptr) {
			for (ImTextureData* texture : *drawData->Textures) {
				if (texture->Status != ImTextureStatus_OK) {
					updateTexture(texture);
				}
			}
		}

		canvas->save();
		// The draw lists are laid out in logical pixels while the surface is physical, so scale before painting.
		canvas->scale(drawData->FramebufferScale.x, drawData->FramebufferScale.y);
		// CmdListsCount is obsolete in 1.92 and left at zero; the live list count is CmdLists.Size.
		for (const ImDrawList* list : drawData->CmdLists) {
			for (const ImDrawCmd& command : list->CmdBuffer) {
				if (command.UserCallback != nullptr) {
					// The reset callback targets state this backend does not keep, so only real callbacks are forwarded.
					if (command.UserCallback != ImDrawCallback_ResetRenderState) {
						command.UserCallback(list, &command);
					}
					continue;
				}
				drawCommand(canvas, list, command, drawData->DisplayPos);
			}
		}
		canvas->restore();
	}
} // namespace example
