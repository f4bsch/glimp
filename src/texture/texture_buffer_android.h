#pragma once

#include "../context.h"
#include "pack_buffer.h"

#ifdef ANDROID
#include<android/hardware_buffer.h>
#include <img/halide.h>

#endif

// see http://snorp.net/2011/12/16/android-direct-texture.html
/*
	Wraps Android's AHardwareBuffer, using EGLImage.
	Currently supports upload (cpu->gpu, unpacking) only!
*/

namespace gl {
	template<typename T=float, int C = 4>
	class texture_buffer_android : public texture_base {
		AHardwareBuffer *hwBuf;
		EGLImageKHR eglImg = 0;
		int hwWidth;
		const buf_use usage;
		GLenum target = GL_TEXTURE_2D;

	public:
		static int nextSupportedWidth(int w) {
			w *= sizeof(T) / 2;
			int o = w % 64;
			if (o != 0) w += 64 - o;
			if (w % 256 == 0) w += 64; // Android does not support 512,1024... TODO
			return w;
		}


		texture_buffer_android(const std::string &name, int width, int height, buf_use use)
				: texture_base(name, width, height),
				  hwWidth(use._forceWidth ? width : nextSupportedWidth(width)), usage(use) {
			static_assert(C == 4, "texture_buffer only supports 4 channels");
			static_assert(!std::is_same<T, float>::value, "texture_buffer_android does not support float32 yet");
			//static_assert(std::is_same<T, float16_t>::value || std::is_same<T, uint16_t>::value,
			//					  "texture_buffer only supports uint16_t, float16_t");

			//if (width < 32)
			//	throw std::runtime_error("row stride too small");

			AHardwareBuffer_Desc desc = {
					.width = static_cast<uint32_t>(hwWidth),
					.height = static_cast<uint32_t>(height),
					.layers = 1
			};

			desc.usage |= use.cpuRead ? AHARDWAREBUFFER_USAGE_CPU_READ_RARELY // TODO rarely/often
									  : AHARDWAREBUFFER_USAGE_CPU_READ_NEVER;
			desc.usage |= use.cpuWrite ? AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN
									   : AHARDWAREBUFFER_USAGE_CPU_WRITE_NEVER;
			desc.usage |= use.gpuRead ? AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE : 0;
			desc.usage |= use.gpuWrite ? AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT : 0;

			uint32_t fmt;
			if (sizeof(T) == 2) {
				desc.format = AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
				if(use.cpuRead) {
					LOG_D << "using R5G6B5 for" << to_string();
					desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
					//desc.format = AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
					desc.width *= 2;
					LOG_D << "allocating AHardwareBuffer " << hwWidth << "x" << height << ", R5G6B5";
				} else {
					LOG_D << "allocating AHardwareBuffer " << hwWidth << "x" << height << ", R16G16B16A16_FLOAT";
				}
			} else {
				throw std::runtime_error("type not implemented");
			}


			// see https://source.android.com/devices/graphics/arch-bq-gralloc

			hwBuf = nullptr;
			if (AHardwareBuffer_allocate(&desc, &hwBuf)) {
				throw std::runtime_error("AHardwareBuffer_allocate failed");
			}

			if (!hwBuf)
				throw std::runtime_error("AHardwareBuffer_allocate failed (buffer NULL)");

			AHardwareBuffer_acquire(hwBuf);

		}

		~texture_buffer_android() {			AHardwareBuffer_release(hwBuf);		}


		void setTargetExternalOES() { target = GL_TEXTURE_EXTERNAL_OES; }

		GLenum getGlTarget() override { return target; };

		void init() override {
			EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID(hwBuf);
			EGLenum surfaceType = EGL_NATIVE_BUFFER_ANDROID;


			// Make an EGL Image at the same address of the native client buffer
			EGLDisplay eglDisplayHandle = eglGetDisplay(EGL_DEFAULT_DISPLAY);


			// Create an EGL Image with these attributes
			// see https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_lock_surface3.txt
			const EGLint eglImageAttributes[] = {
					EGL_WIDTH, (EGLint) hwWidth,
					EGL_HEIGHT, (EGLint) height,
					EGL_MATCH_FORMAT_KHR, EGL_FORMAT_RGBA_8888_KHR, //EGL_FORMAT_RGB_565_KHR,
					EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
					EGL_NONE
			};

			eglImg = eglCreateImageKHR(eglDisplayHandle, EGL_NO_CONTEXT,
									   surfaceType, clientBuffer,
									   eglImageAttributes);
			egl::checkAndThrowError("eglCreateImageKHR");

			if (eglImg == 0)
				throw std::runtime_error("eglCreateImageKHR failed");

			glGenTextures(1, &id);
			gl::context::checkAndThrowError("texture_buffer::init(): glGenTextures");
			glBindTexture(getGlTarget(), id);
			{
				glTexParameteri(getGlTarget(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(getGlTarget(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				//glTexParameteri(getGlTarget(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				//glTexParameteri(getGlTarget(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			gl::context::checkAndThrowError("texture_buffer::init(): glTexParameteri");

			//if(usage.gpuRead) {
			// Attach the EGL Image to the same texture (as if glTexImage)
			// see https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image.txt
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // TODO perf
			glEGLImageTargetTexture2DOES(getGlTarget(), eglImg);
			_glCheckAndThrowError("texture_buffer::init(): glEGLImageTargetTexture2DOES");
			//}


		}

		void _preRender() override {

		}

		void _preRenderTarget() override {
			if(usage.gpuWrite) {
				// glEGLImageTargetRenderbufferStorageOES might be deprecated so this whole block!

				// https://github.com/google/angle/blob/master/src/tests/gl_tests/ImageTest.cpp
				// https://stackoverflow.com/questions/23234006/eglimages-with-renderbuffer-as-source-sibling-and-texture-as-target-sibling

				// !! https://www.0xaa55.com/forum.php?mod=viewthread&tid=896
				GLuint target;
				glGenRenderbuffers(1, &target);
				glPixelStorei(GL_PACK_ALIGNMENT, 4); // TODO perf
				glBindRenderbuffer(GL_RENDERBUFFER, target);
				_glCheckAndThrowError("texture_buffer::init(): glBindRenderbuffer");
				glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, eglImg );
				_glCheckAndThrowError("texture_buffer::init(): glEGLImageTargetRenderbufferStorageOES");
			}
		}

		void deinit() {
			EGLDisplay eglDisplayHandle = eglGetDisplay(EGL_DEFAULT_DISPLAY);
			eglDestroyImageKHR(eglDisplayHandle, eglImg);
		}


		template<uint64_t RW>
		inline T *lock(size_t n) {
			if (n != hwWidth * height * C) {
				LOG_E << "writeStart: expected buffer len " << (width * height * C) << " (" << width
					  << "x" << height << "x" << C << "), got " << n;
				throw std::runtime_error("texture_bufffer:write*(): invalid buf size");
			}
			void *dst = nullptr;
			auto r = AHardwareBuffer_lock(hwBuf, RW, -1, NULL, &dst);
			if (r) std::runtime_error("Error Locking IO output buffer.");
			return reinterpret_cast<T *>(dst);
		}

		inline void unlock() { AHardwareBuffer_unlock(hwBuf, NULL); }

		inline void upload(const T *imgData, size_t n) {
			upload(imgData, n, width);
		}

		inline void upload(const HlBuf<T> &buf) {
			upload(buf.begin(), buf.number_of_elements());
		}

		inline void read(T *imgData, size_t n) {
			download(imgData, n, width);
		}


	private:
		inline void upload(const T *src, size_t n, int rowStride) {
			auto buf = lock<AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN>(n / width * hwWidth);
			for (int y = 0; y < height; y++) {
				memcpy(buf, src, rowStride * C * sizeof(T));
				buf += hwWidth * C,	src += rowStride * C;
			}
			unlock();
		}

		inline void download(T *dst, size_t n, int rowStride) {
			// this might be deprecated
			std::cout << "texBuf: READ" << std::endl;
			auto buf = lock<AHARDWAREBUFFER_USAGE_CPU_READ_RARELY>(n / width * hwWidth);
			for (int y = 0; y < height; y++) {
				memcpy(dst, buf, rowStride * C * sizeof(T));
				buf += hwWidth * C,	dst += rowStride * C;
			}
			unlock();
		}

	public:
		std::string to_string() const override {
			return "texBufAndroid(#" + std::to_string(id) + texture_base::to_string() + ")";
		}
	};
}
