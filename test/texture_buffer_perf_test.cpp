
#ifdef ANDROID
#define DMA_SUPPORT
#include "opengl/texture/texture_buffer_android.h"
#endif

#ifdef DMA_SUPPORT

#include "gtest/gtest.h"

#include "img/halide.h"
#include "opengl/renderer2d.h"
#include "../utils.h"
/*
 * Read about GLES, DMA, Android:
 * https://www.0xaa55.com/forum.php?mod=viewthread&tid=896
 * https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glBufferData.xhtml
 * https://stackoverflow.com/questions/23234006/eglimages-with-renderbuffer-as-source-sibling-and-texture-as-target-sibling
 * https://www.khronos.org/opengl/wiki/Pixel_Buffer_Object
 * https://www.khronos.org/registry/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf
 * https://source.android.com/devices/graphics/arch-bq-gralloc
 * https://android.googlesource.com/platform/packages/experimental/+/ics-mr0/CameraPreviewTest/src/com/example/android/videochatcameratest/SurfaceTextureView.java
 * http://www.engcore.com/2011/10/video-to-texture-streaming-i-mx53-processor/
 * https://www.khronos.org/registry/OpenGL-Refpages/es3.1/
 * https://github.com/google/angle/blob/master/src/tests/gl_tests/ImageTest.cpp
 * https://github.com/google/angle/blob/master/src/tests/gl_tests/SixteenBppTextureTest.cpp
 * https://stackoverflow.com/questions/14102992/gles2-0-use-gl-texture-external-oes-via-gleglimagetargettexture2does
 * https://gist.github.com/rexguo/6696123
 *
 * https://www.khronos.org/files/opengles31-quick-reference-card.pdf
 * https://www.khronos.org/registry/OpenGL/specs/es/3.2/GLSL_ES_Specification_3.20.pdf
 */

TEST(renderer2d_dma_perf, upload1) {
	int w = 1020, h = 3028;
	typedef uint16_t t_out;

	auto shader = R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL";

	HlBuf<uint16_t> hlImg{4, w, h};
	fillRand(hlImg);
	HlBuf<t_out> hlOut{4, w, h};

	Stopwatch sw;
	{
		gl::renderer2d<t_out, 4> renderer(w, h, shader, getContext());
		sw.measure("renderer");

		auto texImg = gl::texture2d<uint16_t, 4>{"texImg", w, h};
		sw.measure("texture");

		renderer.addInput("u_img", texImg);
		renderer.startBackgroundRenderThread(&sw);
		sw.measure("start");

		for (int i = 0; i < 5; i++) {
			texImg.upload(hlImg.begin(), hlImg.number_of_elements());
			sw.measure("upload");
			renderer.render();
			sw.measure("render");
			renderer.getResult(hlOut);
			sw.measure("render");
			sw.measure("non_dma");
		}

		COMP_INT_IMG_FAST(hlOut.begin(), hlImg.begin(), hlImg.number_of_elements());
	}

	{
		gl::renderer2d<t_out, 4> renderer(w, h, shader, getContext());

		auto texImg = gl::texture_dma<uint16_t, 4>{"texImg", w, h, gl::buf_use::to_gpu()};

		renderer.addInput("u_img", texImg);
		renderer.startBackgroundRenderThread(&sw);

		for (int i = 0; i < 5; i++) {
			sw.start();
			texImg.upload(hlImg.begin(), hlImg.number_of_elements());
			renderer.render();
			renderer.getResult(hlOut);
			sw.measure("dma");
		}

		COMP_INT_IMG_FAST(hlOut.begin(), hlImg.begin(), hlImg.number_of_elements());
	}

	//std::cout << sw.getStats() << std::endl;
}



/**
 * Validate that DMA transfer to the GPU is faster
 * For non-dma transfer we use glTexImage2D, for DMA the texture_dma
 */
TEST(renderer2d_dma_perf, upload) {
	int w = 1020, h = 3028;
	int nFrames = 9;
	typedef uint16_t t_out;
	using namespace std::chrono_literals;

	auto shader = R"GLSL(#version 300 es
uniform usampler2D u_img;
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = texelFetch(u_img, pos, 0);
}
)GLSL";

	HlBuf<uint16_t> hlImg{4, w, h};
	HlBuf<t_out> hlOut{4, w, h};

	auto invalidate = [](HlBuf<uint16_t> &img) {
		img.begin()[0] = std::rand() % 1024;
		img.begin()[img.number_of_elements() / 2] = std::rand() % 1024;
		img.begin()[img.number_of_elements() - 1] = std::rand() % 1024;
	};

	fillRand(hlImg);

	Stopwatch sw;
	{
		gl::renderer2d<t_out, 4> renderer(w, h, shader, getContext());
		sw.measure("renderer");

		auto texImg = gl::texture2d<uint16_t, 4>{"texImg", w, h};
		sw.measure("texture");

		renderer.addInput("u_img", texImg);
		renderer.startBackgroundRenderThread(&sw);
		sw.measure("start");


		for (int i = 0; i < nFrames; i++) {
			invalidate(hlImg);
			sw.startGroup("loop");
			sw.start();
			texImg.upload(hlImg.begin(), hlImg.number_of_elements());
			sw.measure("upload");
			renderer.render();
			sw.measure("render");
			sw.measureGroup("loop");
		}


		renderer.getResult(hlOut);
		sw.measure("get");
		COMP_INT_IMG_FAST(hlOut.begin(), hlImg.begin(), hlImg.number_of_elements());


	}
	LOG_V << "Non-DMA Stopwatch:" << std::endl << sw.getStats(1ms) << std::endl;

	fillRand(hlImg);

	Stopwatch sw_dma;
	{
		Stopwatch &sw(sw_dma);

		gl::renderer2d<t_out, 4> renderer(w, h, shader, getContext());
		sw.measure("renderer");

		auto texImg = gl::texture_dma<uint16_t, 4>{"texImg", w, h, gl::buf_use::to_gpu()};
		sw.measure("texture");

		renderer.addInput("u_img", texImg);
		renderer.startBackgroundRenderThread(&sw);
		sw.measure("start");

		for (int i = 0; i < nFrames; i++) {
			invalidate(hlImg);
			sw.startGroup("loop");
			sw.start();
			texImg.upload(hlImg.begin(), hlImg.number_of_elements());
			sw.measure("upload");
			renderer.render();
			sw.measure("render");
			sw.measureGroup("loop");
		}

		renderer.getResult(hlOut);
		sw.measure("get");
		COMP_INT_IMG_FAST(hlOut.begin(), hlImg.begin(), hlImg.number_of_elements());
	}

	LOG_V << "DMA Stopwatch:" << std::endl << sw_dma.getStats(1ms);

	size_t texBytes = sizeof(t_out) * 4 * w * h;
	float megaBytesPerSecond = texBytes / sw.getStatsFor("loop").avg * 1e-6;
	float megaBytesPerSecond_dma = texBytes / sw_dma.getStatsFor("loop").avg * 1e-6;

	float fps = nFrames / sw.getStatsFor("loop").sum;
	float fps_dma = nFrames / sw_dma.getStatsFor("loop").sum;

	EXPECT_GT(megaBytesPerSecond, 160); // peak 450
	EXPECT_GT(megaBytesPerSecond_dma, 900);

	EXPECT_GT(fps, 16);
	EXPECT_GT(fps_dma, 58); // nexus5x_peak=64

	std::cout << "CPU -> GPU Upload (MB/s): " << int(megaBytesPerSecond) << "  - DMA: " << int(megaBytesPerSecond_dma) << std::endl;
	std::cout << "FPS: " << int(fps) << "  - DMA: " << int(fps_dma) << "" << std::endl;

	float dmaSpeedup = sw.getStatsFor("loop").sum / sw_dma.getStatsFor("loop").sum - 1.0f;
	std::cout << "DMA Speedup: " << int(dmaSpeedup * 100) << " %" << std::endl;
	EXPECT_GT(dmaSpeedup, 2.2);
}


TEST(renderer2d_dma_perf, download) {
	using namespace std::chrono_literals;
	int w = 1020, h = 3028;
	int nFrames = 5;
	typedef uint16_t t_out;

	auto shader = R"GLSL(#version 300 es
out uvec4 color;
void main() {
	ivec2 pos = ivec2(gl_FragCoord.xy);
	color = uint(pos.x) + uint(pos.y) * 17U + uvec4(0,1,2,3);
}
)GLSL";

	HlBuf<t_out> hlOut{4, w, h};
	auto ref = [](int c, int x, int y) { return x + y * 17 + c; };

	Stopwatch swTex;



	// currently we have only one candidate for downloading pixels:
	{
		Stopwatch &sw(swTex);
		gl::renderer2d<t_out, 4> renderer(w, h, shader,  getContext());
		renderer.startBackgroundRenderThread(&sw);
		renderer.render();

		fillRand(hlOut);
		for(int i = 0; i < nFrames; i++) {
			sw.start();
			renderer.getResult(hlOut);
			sw.measure("get");
		}
		COMP_RGBA(hlOut, ref, 0.0001);
	}

	size_t texBytes = sizeof(t_out) * 4 * w * h;
	float megaBytesPerSecond = texBytes / swTex.getStatsFor("get").avg * 1e-6;
	float fps = nFrames / swTex.getStatsFor("get").sum;
	std::cout << "GPU -> CPU Download MB/s: " << int(megaBytesPerSecond) << std::endl;
	std::cout << "FPS: " << fps << std::endl;
	EXPECT_GT(megaBytesPerSecond, 400); // nexus5x: >700
	EXPECT_GT(fps, 13); // nexus5x: >27

//	std::cout << "Tex Stopwatch:" << std::endl << swTex.getStats(1ms) << std::endl;
//	std::cout << "TexBuf Stopwatch:" << std::endl << swTexBuf.getStats(1ms) << std::endl;

//	float dmaSpeedup = sw.getStatsFor("loop").sum / sw_dma.getStatsFor("loop").sum - 1.0f;
//	std::cout << "DMA Speedup: " << int(dmaSpeedup * 100) << " %" << std::endl;
//	EXPECT_GT(dmaSpeedup, 2.2);

}

#endif