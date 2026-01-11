#pragma once

#include <raylib.h>
#include <rlgl.h>

#include <stdlib.h>
#include <stdint.h>

/* NOTE: because raylib's default rendertexture struct/functions only let you render to R8G8B8A8 buffers,
 * I'm gonna put together a small rendertexture struct of my own on top of the rlgl.h API so that I can
 * have the ability to render to floating point buffers
 */
struct TextureTarget {
	unsigned int framebuffer;
	unsigned int texture;

	static TextureTarget create(int w, int h, int format = PIXELFORMAT_UNCOMPRESSED_R32) {
		TextureTarget result = { 0 };

		result.texture = create_only_tex(w, h, format);
		result.framebuffer = rlLoadFramebuffer();

		// the one texture in this struct will be the only color attachment, and we have no need for depth/stencil buffer
		// in addition, we're going to attach the texture as a texture2D because other shaders will be sampling them
		rlFramebufferAttach(result.framebuffer, result.texture, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

		return result;
	}

	static void clean(TextureTarget& t) {
		rlUnloadFramebuffer(t.framebuffer);
		rlUnloadTexture(t.texture);

		t.texture = 0;
		t.framebuffer = 0;
	}

	// we probably won't use all of these texture formats, but the initialization for them are here if we ever do
	static int byte_size(int format) {
		switch (format) {
		case PIXELFORMAT_UNCOMPRESSED_R32:
			return sizeof(float);
		case PIXELFORMAT_UNCOMPRESSED_R32G32B32:
			return 3 * sizeof(float);
		case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
			return 4 * sizeof(float);
		case PIXELFORMAT_UNCOMPRESSED_R8G8B8:
			return 3 * sizeof(uint8_t);
		case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
			return 4 * sizeof(uint8_t);
		case PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:
			return sizeof(uint8_t);
		case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
			return 2 * sizeof(uint8_t);
		case PIXELFORMAT_UNCOMPRESSED_R16:
			return sizeof(uint16_t);
		case PIXELFORMAT_UNCOMPRESSED_R16G16B16:
			return 3 * sizeof(uint16_t);
		case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
			return 4 * sizeof(uint16_t);
		default:
			return 0;
		}
	}

	static unsigned int create_only_tex(int w, int h, int format = PIXELFORMAT_UNCOMPRESSED_R32, const void* data = nullptr) {
		unsigned int tex = 0;

		if (!data) {
			void* initialData = calloc(1, w * h * byte_size(format));
			tex = rlLoadTexture(initialData, w, h, format, 0);
			free(initialData);
		} else
			tex = rlLoadTexture(data, w, h, format, 0);

		return tex;
	}
};