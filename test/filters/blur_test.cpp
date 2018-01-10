#include "gtest/gtest.h"
#include "img/halide.h"
#include "opengl/renderer2d.h"
#include "../utils.h"

using namespace glimp;

TEST(filters, blur3x3_r) {
	int w = 32, h = 32;
	typedef uint16_t t_out;

	renderer2d<t_out, 1> renderer(w, h, R"GLSL(#version 300 es
out highp vec4 color;
void main() {
	highp ivec2 size = textureSize(u_img, 0).xy;
	highp ivec2 pos = ivec2(gl_FragCoord.xy);
	uvec4 c0 = texelFetch(u_img, pos, 0);
	uvec4 c1 = texelFetch(u_img, pos + ivec2(1,0), 0);
	color.rgba = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	renderer.startBackgroundRenderThread();
	renderer.render();

	HlBuf<out_t> hlOut{4, w, h};
	renderer.getResult(hlOut);

	auto ref = [w](int c, int x, int y) { return (y * w + x) * 4 + c + 1; };
	COMP_RGBA(hlOut, ref, 0.0001);
}

TEST(renderer2d, download_u32) {
	int w = 21, h = 17;
	typedef uint32_t out_t;

	gl::renderer2d<out_t, 4> renderer(w, h, R"GLSL(#version 300 es
precision highp int; // i32 precision
out uvec4 color;
void main() {
	uvec2 size = uvec2(21,17);
	uvec2 pos = uvec2(gl_FragCoord.xy);
	color = 256U*256U-20U + (pos.y * size.x + pos.x) * 4U + uvec4(1,2,3,4);
}
)GLSL", getContext());

	auto ref = [w](int c, int x, int y) { return 256U * 256U - 20U + (y * w + x) * 4 + c + 1; };
	HlBuf<out_t> hlOut{4, w, h};

	renderer.startBackgroundRenderThread();
	renderer.render();
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, ref, 0.0001);
}


TEST(renderer2d, download_i32) {
	int w = 33, h = 19;
	typedef int32_t out_t;

	gl::renderer2d<out_t, 4> renderer(w, h, R"GLSL(#version 300 es
out mediump ivec4 color;
void main() {
	mediump ivec2 size = ivec2(33,19);
	mediump ivec2 pos = ivec2(gl_FragCoord.xy);
	color = (pos.y * size.x + pos.x) * 4 + ivec4(0,1,2,3) - 100;
}
)GLSL", getContext());

	auto ref = [w](int c, int x, int y) { return (y * w + x) * 4 + c - 100; };
	HlBuf<out_t> hlOut{4, w, h};

	renderer.startBackgroundRenderThread();
	renderer.render();
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, ref, 0.0001);
}


TEST(renderer2d, download_u8) {
	int w = 17, h = 33;
	typedef uint8_t out_t;

	gl::renderer2d<out_t, 4> renderer(w, h, R"GLSL(#version 300 es
out mediump uvec4 color;
void main() {
	mediump ivec2 size = ivec2(17,33);
	mediump ivec2 pos = ivec2(gl_FragCoord.xy);
	color = (uvec4(pos.y * size.x + pos.x) * 4U + uvec4(1,2,3,4)) % 256U;
}
)GLSL", getContext());

	auto ref = [w](int c, int x, int y) {
		return ((y * w + x) * 4 + c + 1) % 256U;
	};

	HlBuf<out_t> hlOut{4, w, h};

	renderer.startBackgroundRenderThread();
	renderer.render(), renderer.getResult(hlOut);

	COMP_RGBA(hlOut, ref, 0.0001);
}


TEST(renderer2d, download_u16) {
	int w = 32, h = 37;
	typedef uint16_t out_t;

	gl::renderer2d<out_t, 4> renderer(w, h, R"GLSL(#version 300 es
out mediump uvec4 color;
void main() {
	mediump ivec2 size = ivec2(32,37);
	mediump ivec2 pos = ivec2(gl_FragCoord.xy);
	color = (uvec4(pos.y * size.x + pos.x) * 4U + uvec4(1,2,3,4));
}
)GLSL", getContext());

	auto ref = [w](int c, int x, int y) {
		return ((y * w + x) * 4 + c + 1);
	};

	HlBuf<out_t> hlOut{4, w, h};

	renderer.startBackgroundRenderThread();
	renderer.render(), renderer.getResult(hlOut);

	COMP_RGBA(hlOut, ref, 0.0001);
}


TEST(renderer2d, tex_u16_to_f32) {
	int w = 16, h = 16;
	typedef uint16_t tex_t;

	gl::renderer2d<float, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out vec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color.rgba = 0.5 + vec4(texelFetch(u_img, pos, 0));
}
)GLSL", getContext());

	gl::texture2d<tex_t, 4> texImg{"texImg", w, h};
	renderer.addInput("u_img", texImg);

	HlBuf<tex_t> hlImg{4, w, h};
	fillRand(hlImg, 1000);
	texImg.upload(hlImg);

	renderer.startBackgroundRenderThread();
	renderer.render();

	HlBuf<float> hlOut{4, w, h};
	renderer.getResult(hlOut);

	auto ref = [&hlImg](int c, int x, int y) { return 0.5 + hlImg(c, x, y); };

	float E = 0.0001f;
	COMP_RGBA(hlOut, ref, E);
}


TEST(renderer2d, tex_u8_to_u8) {
	int w = 121, h = 97;
	gl::renderer2d<uint8_t, 4> renderer(w, h, R"GLSL(#version 300 es
uniform mediump usampler2D u_img;
out mediump uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	gl::texture2d<uint8_t, 4> texImg{"texImg", w, h};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	// generate test image and upload
	HlBuf<uint8_t> hlImg{4, w, h};
	fillRand(hlImg);
	texImg.upload(hlImg);

	renderer.render();
	HlBuf<uint8_t> hlOut{4, w, h};
	renderer.getResult(hlOut);

	COMP_RGBA(hlOut, hlImg, 0);
}


TEST(renderer2d, tex2d_u8_mb_input) {
	int w = 16, h = 16;
	// this tests multi-byte sampling. we store u16 RGBA data and sample with a u8 RGBA texture
	// why? to validate the sampling model using a texture format constraint of u8 and HDR data
	gl::renderer2d<float, 4> renderer(w, h, R"GLSL(#version 300 es
uniform mediump usampler2D u_img;
out mediump vec4 color;

mediump uvec4 fetch_u16(ivec2 p) {
    uvec4 v1 = texelFetch(u_img, ivec2(p.x*2+0, p.y), 0);
    uvec4 v2 = texelFetch(u_img, ivec2(p.x*2+1, p.y), 0);
    return uvec4(
        v1.r + v1.g * 256U,
        v1.b + v1.a * 256U,
        v2.r + v2.g * 256U,
        v2.b + v2.a * 256U
      );
}

void main() {
	ivec2 size = textureSize(u_img, 0).xy;
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color.rgba = vec4(fetch_u16(pos));
}
)GLSL", getContext());

	// pack u16 data in u8 texture with double width
	gl::texture2d<uint8_t, 4> texImg{"texImg", w * 2, h};
	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	// generate test u16 image
	HlBuf<uint16_t> hlImg{4, w, h};
	fillRand(hlImg, 2000);
	texImg.upload(reinterpret_cast<uint8_t *>(hlImg.begin()), hlImg.number_of_elements() * 2);

	renderer.render();

	HlBuf<float> hlOut{4, w, h};
	renderer.getResult(hlOut);

	auto ref = [&hlImg](int c, int x, int y) {
		return hlImg(c, x, y);
	};

	float E = 0.001f;
	EXPECT_NEAR(hlOut(0, 0, 0), ref(0, 0, 0), E);
	EXPECT_NEAR(hlOut(1, 0, 0), ref(1, 0, 0), E);
	EXPECT_NEAR(hlOut(2, 0, 0), ref(2, 0, 0), E);
	EXPECT_NEAR(hlOut(3, 0, 0), ref(3, 0, 0), E);

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			for (int c = 0; c < 4; ++c) {
				ASSERT_NEAR(hlOut(c, x, y), ref(c, x, y), E);
			}
		}
	}
}


TEST(renderer2d, tex_u16_to_u16_12MPIX) {
	Stopwatch sw;

	int w = 1020, h = 3028;
	typedef uint16_t tex_t;

	gl::renderer2d<uint16_t, 4> renderer(w, h, R"GLSL(#version 300 es
uniform usampler2D u_img;
out mediump uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color.rgba = 1U + texelFetch(u_img, pos, 0);
}
)GLSL", getContext());

	gl::texture2d<tex_t, 4> texImg{"texImg", w, h};
	renderer.addInput("u_img", texImg);
	renderer.startBackgroundRenderThread(&sw);

	HlBuf<tex_t> hlImg{4, w, h};
	fillRand(hlImg, 1000);
	texImg.upload(hlImg);
	sw.measure("init");

	renderer.render();
	sw.measure("render");

	HlBuf<uint16_t> hlOut{4, w, h};
	renderer.getResult(hlOut);
	sw.measure("download");

	auto ref = [&hlImg](int c, int x, int y) {
		return 1 + hlImg(c, x, y);
	};
	COMP_RGBA(hlOut, ref, 0.0001);

	LOG_I << "tex_u16_to_u16_12MPIX" << std::endl << sw.getStats();
}

TEST(renderer2d, tex_u32_in_u16) {
	int w = 320, h = 240;
	// test multi-byte sampling. we store u32 RGBA data and sample with a u16 RGBA texture
	gl::renderer2d<uint32_t, 4> renderer(w, h, R"GLSL(#version 300 es
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
)GLSL", getContext());

	// pack u32 data in u16 texture with double width
	gl::texture2d<uint16_t, 4> texImg{"texImg", w * 2, h};
	//gl::texture_buffer<uint16_t, 4> texImg{"texImg", w*2, h, gl::buf_use::to_gpu()};

	renderer.addInput("u_img", texImg);

	renderer.startBackgroundRenderThread();

	// generate test u32 image
	HlBuf<uint32_t> hlImg{4, w, h};
	fillSweep(hlImg, 65536 - 16); // sweep from short to above-short range
	texImg.upload(reinterpret_cast<uint16_t *>(hlImg.begin()), hlImg.number_of_elements() * 2);

	renderer.render();

	HlBuf<uint32_t> hlOut{4, w, h};
	renderer.getResult(hlOut);

	auto ref = [&hlImg](int c, int x, int y) { return hlImg(c, x, y); };
	COMP_RGBA(hlOut, ref, 0.0001);
}