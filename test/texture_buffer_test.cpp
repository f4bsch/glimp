
#ifdef ANDROID
#define DMA_SUPPORT
#include "opengl/texture/texture_buffer_android.h"
#endif

#ifdef DMA_SUPPORT
#include "gtest/gtest.h"

#include "img/halide.h"
#include "opengl/renderer2d.h"
#include "../utils.h"

/**
 * Test half-float conversion
 */
TEST(renderer2d_dma, float16) {
	std::srand(std::time(nullptr));
	for (int i = 0; i < 100; i++) {
		float f = static_cast<float>(std::rand() % 1000) * 0.1;
		ASSERT_NEAR(f, f16_to_float(float_to_f16(f)), 0.1);
	}
}

/**
 * Render a DMA texture with 1pixel height
 * Scope: DMA data upload
 */
TEST(renderer2d_dma, tex_u16_line) {
	int w = 1200, h = 1;
	typedef uint16_t t_out, t_tex;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out highp uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = uvec4(texelFetch(u_img, pos, 0));
}
)GLSL", getContext());

	gl::texture_dma <t_tex, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu()};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	HlBuf<t_tex> hlImg{4, w, h};
	std::srand(std::time(nullptr));
	fillRand(hlImg, 32000);
	texImg.upload(hlImg.begin(), hlImg.number_of_elements());

	renderer.render();
	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, hlImg, 0.0001f);
}


/**
 * Use a DMA texture of half-floats, sample it using samplerExternalOES
 * Scope: samplerExternalOES, half-float, 2D DMA texture sampling
 */
TEST(renderer2d_dma, tex_ext_f16_to_u16) {
	int w = 32, h = 4;
	typedef uint16_t t_out;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
uniform samplerExternalOES u_img;
out highp uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = uvec4(texelFetch(u_img, pos, 0));
}
)GLSL", getContext());

	gl::texture_buffer_android<gl::float16_t, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu()};
	texImg.setTargetExternalOES();
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	auto ref = [w](int c, int x, int y) {	return (y * w + x) * 4 + c;	};

	// generate test image and upload
	HlBuf<uint16_t> hlImg{4, w, h};
	std::srand(std::time(nullptr));
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			for (int c = 0; c < 4; c++)
				hlImg(c, x, y) = float_to_f16(ref(c, x, y));
		}
	}

	texImg.upload(reinterpret_cast<gl::float16_t *>(hlImg.begin()), hlImg.number_of_elements());

	renderer.render();

	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);
	COMP_RGBA(hlOut, ref, 0.0001);
}


/**
 * Use a DMA texture of uint16, sample it using usampler2D
 * Scope: 2D uint16 DMA texture sampling
 */
TEST(renderer2d_dma, tex_u16_to_u16) {
	int w = 640, h = 320;
	typedef uint16_t t_out;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 310 es
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


TEST(renderer2d_dma, tex_u16_to_u32) {
	int w = 640, h = 320;
	typedef uint32_t t_out;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 310 es
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

	HlBuf<uint16_t> hlImg{4, w, h};
	fillRand(hlImg);
	texImg.upload(hlImg.begin(), hlImg.number_of_elements());

	renderer.render();
	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);
	COMP_RGBA(hlOut, hlImg, 0.0001);
}


TEST(renderer2d_dma, tex_u16_to_u32_ma) {
	int w = 640, h = 320;
	typedef uint32_t t_out;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 310 es
uniform mediump usampler2D u_img;
out highp uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0) * 739U;
	color += 1337U;
}
)GLSL", getContext());

	gl::texture_dma<uint16_t, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu()};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	HlBuf<uint16_t> hlImg{4, w, h};
	fillRand(hlImg);
	texImg.upload(hlImg.begin(), hlImg.number_of_elements());

	renderer.render();
	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);

	auto ref = [hlImg](int c, int x, int y) {	return hlImg(c,x,y) * 739U + 1337U;	};
	COMP_RGBA(hlOut, ref, 0.0001);
}

/**
 * Use a DMA texture with a width that is not aligned to 64-pixel
 * We copy line-wise into the texture virtual memory
 */
TEST(renderer2d_dma, tex_u16_to_u16_unal) {
	int w = 1020, h = 32;
	typedef uint16_t t_out;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
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
	std::srand(std::time(nullptr));
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			for (int c = 0; c < 4; c++)
				hlImg(c, x, y) = (y * w + x) * 4 + c; //std::rand() % std::numeric_limits<uint16_t>::max();
		}
	}

	texImg.upload(hlImg.begin(), hlImg.number_of_elements());
	renderer.render();
	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, hlImg, 0.0001);
}

/**
 * Generate a 12MPIX (RAW bayer) image, render and download
 */
TEST(renderer2d_dma, tex_u16_12MPIX) {
	Stopwatch sw;
	int w = 1024 + 64, h = 3028;
	typedef uint16_t t_out;

	int n = w * h * 4;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	gl::texture_dma<uint16_t, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu()};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread(&sw);

	std::vector<uint16_t> img(n);
	tools::fastRand fr;
	auto imgDataEnd = img.data() + img.size();
	for (auto ip = img.data(); ip != imgDataEnd; ip += 2) {
		*reinterpret_cast<uint32_t *>(ip) = fr.next();
	}
	sw.measure("gen");

	texImg.upload(img.data(), img.size());
	sw.measure("upload");

	renderer.render();
	sw.measure("render");

	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);
	sw.measure("download");

	COMP_INT_IMG_FAST(img.data(), hlOut.begin(), img.size());

	// render a 2nd time for stats (first render might take extra init time)
	sw.start();
	renderer.render();
	sw.measure("render");

	LOG_I << sw.getStats();
}


/**
 * Generate a 12MPIX (RAW bayer) image, render and download
 */
TEST(renderer2d_dma, tex_u16_12MPIX_unaligned) {
	Stopwatch sw;
	int w = 1020, h = 3028;
	typedef uint16_t t_out;

	int n = w * h * 4;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	gl::texture_dma<uint16_t, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu()};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread(&sw);

	// generate test image and upload
	std::vector<uint16_t> img(n);
	tools::fastRand fr;
	auto imgDataEnd = img.data() + img.size();
	for (auto ip = img.data(); ip != imgDataEnd; ip += 2) {
		*reinterpret_cast<uint32_t *>(ip) = fr.next();
	}
	sw.measure("gen");

	texImg.upload(img.data(), img.size());
	sw.measure("upload");

	renderer.render();
	sw.measure("render");

	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);
	sw.measure("download");


	auto imgData = img.data(), outData = hlOut.begin();
	int res;
	EXPECT_EQ(0, res = memcmp(imgData, outData, n * sizeof(*imgData)));
	sw.measure("memcmp");

	if (res) {
		for (int i = 0; i < n; i++) {
			ASSERT_EQ(imgData[i], outData[i]);
		}
		sw.measure("compare");
	}

	// render a 2nd time for stats (first render might take extra init time)
	renderer.render();
	sw.measure("render");

	LOG_I << "tex_u16_12MPIX_unaligned:" << std::endl << sw.getStats();
}

/**
* Finds what widths are valid for DMA
*/
TEST(renderer2d_dma, tex_u16_to_u16_valid_w) {
	int h = 4;
	typedef uint16_t t_out;

	std::vector<int> valid;

	// 32, 64*N, but NOT valid: 512, 768, 1024, 1280, 1536

	for(int w = 960; w < 2000; w+=16) {
		gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

		gl::texture_dma<uint16_t, 4> texImg{"texImg", w, h, gl::buf_use::to_gpu().forceWidth()};
		renderer.addInput("u_img", texImg);

		renderer.startBackgroundRenderThread();

		HlBuf<uint16_t> hlImg{4, w, h};
		fillRand(hlImg);

		texImg.upload(hlImg.begin(), hlImg.number_of_elements());

		renderer.render();

		HlBuf<t_out> hlOut{4, w, h};
		renderer.getResult(hlOut);
		int res = memcmp(hlImg.begin(), hlOut.begin(), hlImg.size_in_bytes());

		if(res == 0) {
			valid.push_back(w);
		}
	}
	ASSERT_EQ(960, valid[0]);
	ASSERT_EQ(1088, valid[1]);

	LOG_I << "Valid DMA widths: " << valid;
}



/**
 * Use a DMA texture of uint16, sample it using usampler2D
 * Scope: 2D uint16 DMA texture sampling
 */
/*
TEST(renderer2d_dma, tex_f32_to_f32) {
	int w = 640, h = 320;
	typedef float t_out;

	gl::renderer2d<t_out, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	gl::texture_dma<float, 4> texImg{"texImg", w, h};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	// generate test image and upload
	HlBuf<float> hlImg{4, w, h};
	fillRand(hlImg, 1000);
	texImg.upload(hlImg.begin(), hlImg.number_of_elements());

	renderer.render();
	HlBuf<t_out> hlOut{4, w, h};
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, hlImg, 0.0001);
}
 */


TEST(renderer2d_dma, tex_u32_packed_in_u16) {
	int w = 320, h = 240;
	ASSERT_GT(w*h, std::numeric_limits<uint16_t>::max());

	auto shader1 = R"GLSL(#version 300 es
uniform highp usampler2D u_img;
out highp uvec4 color;
highp uvec4 fetch_u32(ivec2 p) {
    highp uvec4 v1 = texelFetch(u_img, ivec2(p.x*2+0, p.y), 0);
    highp uvec4 v2 = texelFetch(u_img, ivec2(p.x*2+1, p.y), 0);
    return uvec4(
        v1.r + v1.g * 65536U, v1.b + v1.a * 65536U,
        v2.r + v2.g * 65536U, v2.b + v2.a * 65536U
    );
}
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = fetch_u32(pos);
}
)GLSL";

	auto shader2 = R"GLSL(#version 300 es
#define PREC highp

precision PREC float;
precision PREC int;
precision PREC sampler2D;
precision PREC usampler2D;
precision PREC isampler2D;

uniform mediump  usampler2D u_img;
out highp uvec4 color;
#define U 65536U
highp uvec4 fetch_u32(ivec2 p) {
    mediump uvec4 v1 = texelFetch(u_img, ivec2(p.x*2+0, p.y), 0);
    mediump uvec4 v2 = texelFetch(u_img, ivec2(p.x*2+1, p.y), 0);
	//highp uvec4 r = highp uvec4(v1.g, v1.a, v2.g, v2.a) * U;
	//return highp uvec4(v1.r, v1.b, v2.r, v2.b);
	//return r;
    return highp uvec4( //0U,1U,2U,3U);
        v1.r + v1.g * U, v1.b + v1.a * U,
        v2.r + v2.g * U, v2.b + v2.a * U
    );
}
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = fetch_u32(pos);
}
)GLSL";

	auto shader3 = R"GLSL(#version 300 es
uniform mediump  usampler2D u_img;
out highp uvec4 color;
#define U 65536U
highp uvec4 fetch_u32(ivec2 p) {
    mediump uvec4 v1 = texelFetch(u_img, ivec2(p.x*2+0, p.y), 0);
    mediump uvec4 v2 = texelFetch(u_img, ivec2(p.x*2+1, p.y), 0);
    return highp uvec4(
        v1.r + v1.g * U, v1.b + v1.a * U,
        v2.r + v2.g * U, v2.b + v2.a * U
    );
}
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = fetch_u32(pos);
}
)GLSL";

	// this tests multi-byte sampling. we store u32 RGBA data and sample with a u16 RGBA texture
	gl::renderer2d<uint32_t, 4> renderer(w, h, shader3, getContext());

	// pack u32 data in u16 texture with double width
	gl::texture_dma<uint16_t, 4> texImg{"texImg", w * 2, h, gl::buf_use::to_gpu()};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	// generate test u32 image
	HlBuf<uint32_t> hlImg{4, w, h};
	fillSweep(hlImg, 65536 - 16); // sweep from short to above-short range
	hlImg(1,0,0) = 183;
	fillSweep(hlImg, 1);
	texImg.upload(reinterpret_cast<uint16_t *>(hlImg.begin()), hlImg.number_of_elements() * 2);

	renderer.render();

	HlBuf<uint32_t> hlOut{4, w, h};
	renderer.getResult(hlOut);

	auto ref = [&hlImg](int c, int x, int y) { return hlImg(c, x, y); };
	COMP_RGBA(hlOut, ref, 0.0001);
}


#endif