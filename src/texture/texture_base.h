#pragma once

#include "../context.h"

namespace gl {
	/*
	struct boundary_condition {
		GLint param;

		static boundary_condition mirror_interior() {
			GL_CLAMP_TO_EDGE
			GL_CLAMP_TO_EDGE
		}
		
		
	};
	 */


	class texture_base {
	protected:
		texture_base(const std::string &name, int width, int height)
				: name(name), width(width), height(height) {}

		GLuint id = 0;


		inline void maybeInit() { if (id == 0) init(); }

	public:
		const std::string name;
		const int width, height;

		virtual std::string to_string() const {
			return "texBase('" + name + "', #" + std::to_string(id) + ", "
				   + std::to_string(width) + "x" + std::to_string(height) + ")";
		};

		virtual void init() = 0;

		inline GLuint getGlId() {
			if (id == 0) init();
			return id;
		}

		virtual void _preRender() {};
		virtual void _preRenderTarget() {};

		const texture_base &operator=(const texture_base &o) {
			return o;
		}

		virtual GLenum getGlTarget() { return GL_TEXTURE_2D; };

		std::string getGlTargetString() {
			switch (getGlTarget()) {
				case GL_TEXTURE_2D:
					return "GL_TEX*_2D";
				case GL_TEXTURE_EXTERNAL_OES:
					return "GL_TEX*_EXT*_OES";
				default:
					return "<unknown " + std::to_string(getGlTarget()) + ">";
			}
		}

		void _glCheckAndThrowError(const std::string &msg) {
			gl::context::checkAndThrowError(msg, *this);
		}
	};
}