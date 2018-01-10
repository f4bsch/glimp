#pragma once

#include "../context.h"
#include "texture2d.h"

namespace gl {
	template<typename T, int C>
	struct pack_buffer {
		GLuint pbo = 0;
		std::function<void(void)> init;
		size_t sizeBytes;

		pack_buffer(int width, int height, buf_use use)
				: sizeBytes(sizeof(T) * C * width * height) {
			init = [=]() {
				LOG_I << "init pack_buffer";
				glGenBuffers(1, &pbo);
				gl::context::checkAndThrowError("pack_buffer glGenBuffers");

				if (use.cpuRead) {
					glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
					glBufferData(GL_PIXEL_PACK_BUFFER, sizeBytes, 0, GL_DYNAMIC_READ); // TODO
					glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
				}

				if (use.cpuWrite) {
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
					glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeBytes, 0, GL_DYNAMIC_DRAW); // TODO
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
					LOG_D << "initalized unpack buffer " << pbo << " with size " << sizeBytes;
				}

				gl::context::checkAndThrowError("renderer2d glBufferData");
			};
		}


		void read(texture2d <T, C> &tex, std::function<void(const T *)> &reader,
				  Stopwatch *sw = nullptr) {
			glReadBuffer(GL_COLOR_ATTACHMENT0);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
			tex.read(nullptr, sizeBytes / sizeof(T)); // as glReadPixels(..., 0);
			if (sw)sw->measure("texRead");
			auto ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sizeBytes, GL_MAP_READ_BIT);
			if (sw)sw->measure("glMapBufferRange");
			reader(reinterpret_cast<T *>(ptr));
			//memcpy(dst, ptr, nBytes);
			//if (sw)sw->measure("pbo_memcpy");
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			if (sw)sw->measure("glUnmapBuffer");
		}

		void write(texture2d <T, C> &tex, std::function<void(T *)> &writer) {
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
			gl::context::checkAndThrowError("pack_buffer::write(): glBindBuffer");

			glBindTexture(GL_TEXTURE_2D, tex.getGlId());

			// TODO glMapBuffer 
			auto ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, sizeBytes, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
			gl::context::checkAndThrowError("pack_buffer::write(): glMapBufferRange" + std::to_string(sizeBytes));
			writer(reinterpret_cast<T *>(ptr));
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			gl::context::checkAndThrowError("pack_buffer::write(): glUnmapBuffer");

			gl::context::checkAndThrowError("pack_buffer::write(): glBindTexture");
			tex.write(nullptr, sizeBytes / sizeof(T));
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}

		virtual void deinit() { glDeleteBuffers(1, &pbo); }
	};
}