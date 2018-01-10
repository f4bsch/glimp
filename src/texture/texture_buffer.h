#pragma once

#include "context.h"
#include "pack_buffer.h"

#ifdef ANDROID

#include<android/hardware_buffer.h>

#endif

// see http://snorp.net/2011/12/16/android-direct-texture.html

namespace gl {

#ifdef ANDROID
	template<typename T=float, int C = 4>
	class texture_buffer : public texture2d {
		pack_buffer<T,C> pb;

	public:
		texture_buffer(const std::string &name, int width, int height, buf_use use)
				: texture2d(name, width, height), pb(width, height, use) {	}

		~texture_buffer() {	}

		void init() override {
			texture2d::init();
			pb.init();
		}

		void deinit() {

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
			return "texBufU8(#" + std::to_string(id) + texture_base::to_string() + ")";
		}
	};

	template<typename T=float, int C = 4>
	class texture_buffer_to_cpu : public texture_buffer<T, C> {
	public:
		explicit texture_buffer_to_cpu(const std::string &name, int width, int height)
				: texture_buffer<T, C>(name, width, height, buf_use::to_cpu()) {}
	};
}
