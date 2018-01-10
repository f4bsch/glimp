#pragma once

#include "texture/texture_pbo.h"

namespace gl {

	/*
	 * Wraps a framebuffer, texture and pack buffer for direct read access to drawn pixels
	 */
	template<typename T, int C>
	struct render_target {
		GLuint fb;
		texture_pbo<T,C> texBuf;

		render_target(int width, int height)
				: texBuf{"out", width, height, buf_use::to_cpu()} {}

		inline int width() { return texBuf.width; }
		inline int height() { return texBuf.height; }

		void init() {
			glGenFramebuffers(1, &fb);
			gl::context::checkAndThrowError("renderer2d GenFramebuffers");

			texBuf.init();
		}

		void bind(bool checkCompleteness = false) {
			glBindFramebuffer(GL_FRAMEBUFFER, fb);
			texBuf._preRenderTarget();

			if(checkCompleteness) {
				auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				gl::context::checkAndThrowError("render(): CheckFramebufferStatus");
				if (status != GL_FRAMEBUFFER_COMPLETE) {
					LOG_E << "GL FBO " << fb << " setup failed (" << status << ") texture:"
						  << texBuf.to_string();
					throw std::runtime_error("framebuffer error");
				}
				//LOG_V << "gl::renderer2d(frame=" << frameIndex << "): framebuffer complete with target "
				//	  << target.tex.to_string();
			}
		}

		void clear() {
			bind();
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texBuf.getGlTarget(), texBuf.getGlId(), 0);
			gl::context::checkAndThrowError("render(): glFramebufferTexture2D ", texBuf);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		void read(std::function<void(const T *src)> &reader, int n) {
			size_t nBytes = sizeof(T) * C * texBuf.width * texBuf.height;
			if (n * sizeof(T) != nBytes) {
				throw std::runtime_error("render_target::read(): invalid buf size");
			}
			texBuf.read(reader, n);
		}


		void read(T *dst, int n) {
			size_t nBytes = sizeof(T) * C * texBuf.width * texBuf.height;
			if (n * sizeof(T) != nBytes) {
				throw std::runtime_error("render_target::read(): invalid buf size");
			}
			std::function<void(const T *src)> reader = [dst, nBytes](const T *src) { memcpy(dst, src, nBytes); };
			texBuf.read(reader, n);
		}
	};
}