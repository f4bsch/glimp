/*
 * This might be deprecated, see below
 */

#ifdef ANDROID
#define DMA_SUPPORT
#endif

#ifdef DMA_SUPPORT

#include "gtest/gtest.h"

#include "img/halide.h"
#include "opengl/renderer2d.h"
#include "../utils.h"

/**
 * Client buffer read/write (no GL)
 */
TEST(renderer2d_dma_duplex, rw_buffer) {
	int h = 64;
	for(int w = 64; w <= 128; w+=4) {
		gl::texture_dma<uint16_t, 4> tb{"tb", w, h, gl::buf_use::duplex()};

		HlBuf<uint16_t> refData{4, w, h};
		fillRand(refData);

		HlBuf<uint16_t> dlData{4, w, h};

		tb.upload(refData.begin(), refData.number_of_elements());
		tb.read(dlData.begin(), dlData.number_of_elements());

		COMP_RGBA(refData, dlData, 0);
	}
}


/**
 * Use a DMA texture of uint16, sample it using usampler2D
 * Scope: 2D uint16 DMA texture sampling
 *
 * DEPRECATED? this uses texture_dma_to_cpu as render target, which relies on
 * glEGLImageTargetRenderbufferStorageOES, which seems to be deprecated. since GLES 3 we can use PBO
 * for transfer, which we do by default
 */
/*
TEST(renderer2d_dma_duplex, u16_to_u16) {
	int w = 640, h = 320;
	typedef uint16_t t_out;

	gl::renderer2d<t_out, 4, gl::texture_dma_to_cpu<t_out, 4>> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	gl::texture_dma<uint16_t, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu()};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

// generate test image and upload
	HlBuf<uint16_t> hlImg{4, w, h};
	fillRand(hlImg);
	texImg.upload(hlImg.begin(), hlImg.number_of_elements());

	renderer.render();
	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, hlImg, 0.0001);
}
*/

#endif