#pragma once

#include "img/halide.h"
#include "../context.h"
#include "texture_base.h"

namespace gl {
	template<typename F, int C>
	class texture2d : public texture_base {
	public:
		typedef F type;

	private:
		const F *uploadData;
		bool hostDirty = true;
		bool externalOES = false;
	protected:
		int rowAlign;

		virtual void doUpload(const F *data) {
			if (externalOES)
				throw std::runtime_error("cannot upload to external OES " + to_string());


			glPixelStorei(GL_UNPACK_ALIGNMENT, rowAlign); // for upload glTexImg* TODO perf
			glPixelStorei(GL_PACK_ALIGNMENT, rowAlign); // for download glReadPixel* TODO perf
			gl::context::checkAndThrowError("textured2d::write(): glPixelStorei ", *this);

			glTexImage2D(GL_TEXTURE_2D, 0, getGlSizedFormat(), width, height, 0, getGlFormat(),
						 getGlType(), (const void *) data);

			gl::context::checkAndThrowError("textured2d::write(): glTexImage2D with ", *this);

			if (data == 0) {
				LOG_V << "initialized " << to_string() << ", rowAlign=" << rowAlign;
			}
		}

	public:
		void makeExternalOES() {
			if (id != 0)
				throw std::runtime_error("texture2d::makeExternalOES(): already initialized");
			externalOES = true;
		}

		void _preRender() override {
			if (hostDirty) {
				doUpload(uploadData);
				hostDirty = false;
			}
		}

	public:
		texture2d(const std::string name, int width, int height)
				: texture_base(name, width, height), uploadData(nullptr) {
			int rowStride = width * sizeof(F) * C;
			if (height > 2 && rowStride < 16 * 1 * 4)
				throw std::runtime_error("row stride too small!");
			for (int i = 8; i >= 1; i /= 2) {
				if (rowStride % i == 0) {
					rowAlign = i;
					break;
				}
			}
		}

		void upload(const F *data, size_t n) {
			if (n != (width * height * C))
				throw std::runtime_error("texture2d::upload(): invalid data length");

			if (externalOES)
				throw std::runtime_error("cannot upload to external OES " + to_string());

			uploadData = data;
			hostDirty = true;
		}

		inline void upload(const HlBuf<F> &buf) {
			//if(buf.width() != width || buf.height() != height) {
			//	throw std::runtime_error("texture2d::upload(): invalid buffer extent");
			//}
			upload(buf.begin(), buf.number_of_elements());
		}

		//void uploadSynced(const F *data, size_t dataLen, gl::context &gl) {
//			upload(data, dataLen);
		//}


		virtual void read(F *buf, int n) {
			// supported types: GL_UNSIGNED_BYTE, GL_UNSIGNED_INT, GL_INT, or GL_FLOAT
			glPixelStorei(GL_PACK_ALIGNMENT, rowAlign);
			GLenum format = isInteger() ? GL_RGBA_INTEGER : GL_RGBA;
			glReadPixels(0, 0, width, height, format, getGlType(), buf);
			context::checkAndThrowError("texture::read(): glReadPixels ", *this);
		}

		virtual void write(const F *buf, int n) {
			if (externalOES)
				throw std::runtime_error("cannot write to external OES " + to_string());
			glPixelStorei(GL_UNPACK_ALIGNMENT, rowAlign);
			glTexImage2D(GL_TEXTURE_2D, 0, getGlSizedFormat(), width, height, 0, getGlFormat(),
						 getGlType(), (const void *) buf);
			gl::context::checkAndThrowError("textured2d::write(): glTexImage2D with ", *this);
		}


		GLenum getGlTarget() override {
			// GL_TEXTURE_EXTERNAL_OES is from OES_EGL_image_external
			// see https://www.khronos.org/registry/OpenGL/extensions/OES/OES_EGL_image_external.txt
			return externalOES ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
		}

		std::string to_string() const override {
			return "tex2d(" + texture_base::to_string() + "," + getGlSizedFormatString() +
				   (externalOES ? ", OES" : "") + ")";
		}

	protected:
		void init() override {
			glGenTextures(1, &id);
			gl::context::checkAndThrowError("texture(): glGenTextures");

			glBindTexture(getGlTarget(), id);
			gl::context::checkAndThrowError("texture(): glBindTexture", *this);

			{
				glTexParameteri(getGlTarget(), GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(getGlTarget(), GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(getGlTarget(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(getGlTarget(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				gl::context::checkAndThrowError("texture(): glTexParameteri");
			}

			if (!externalOES) {
				// init the texture (set size, type and format)
				glTexImage2D(getGlTarget(), 0, getGlSizedFormat(), width, height, 0, getGlFormat(),
							 getGlType(), 0);
			} else {
				// see http://developer.android.com/reference/android/graphics/SurfaceTexture.html
				// external textures will be updated externaly
			}
		}

		virtual void deinit() {
			glDeleteTextures(1, &id);
		}

		bool isInteger() const {
			return typeid(F) != typeid(float) && typeid(F) != typeid(double);
		}

		GLenum getGlType() {
			if (typeid(F) == typeid(float)) {
				return GL_FLOAT;
			} else if (typeid(F) == typeid(uint8_t)) {
				return GL_UNSIGNED_BYTE;
			} else if (typeid(F) == typeid(uint32_t)) {
				return GL_UNSIGNED_INT;
			} else if (typeid(F) == typeid(int32_t)) {
				return GL_INT;
			} else if (typeid(F) == typeid(uint16_t)) {
				return GL_UNSIGNED_SHORT;
			} else if (typeid(F) == typeid(int16_t)) {
				return GL_SHORT;
			}
			throw std::runtime_error("invalid texture type");
		}


#define RETURN_FROM_TYPE_GROUP(ARRAY_SET_NAME, NAME_OR_VALUE) int i = (C <= 1) ? 0 : (C - 1); \
        if (typeid(F) == typeid(float)) return ARRAY_SET_NAME ## _f32[i].NAME_OR_VALUE; \
        else if (typeid(F) == typeid(uint32_t)) return ARRAY_SET_NAME ## _u32[i].NAME_OR_VALUE; \
        else if (typeid(F) == typeid(int32_t)) return ARRAY_SET_NAME ## _i32[i].NAME_OR_VALUE; \
        else if (typeid(F) == typeid(uint16_t)) return ARRAY_SET_NAME ## _u16[i].NAME_OR_VALUE; \
        else if (typeid(F) == typeid(int16_t)) return ARRAY_SET_NAME ## _i16[i].NAME_OR_VALUE; \
        else if (typeid(F) == typeid(uint8_t)) return ARRAY_SET_NAME ## _u8[i].NAME_OR_VALUE; \
        else throw std::runtime_error("invalid texture format");

		GLenum getGlSizedFormat() const {
			RETURN_FROM_TYPE_GROUP(sfmts, value);
		}

		std::string getGlSizedFormatString() const {
			RETURN_FROM_TYPE_GROUP(sfmts, name);
		}


		GLenum getGlFormat() const {
			static GLenum
					fmt_float[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA},
					fmt_integer[] = {GL_RED_INTEGER, GL_RG_INTEGER, GL_RGB_INTEGER,
									 GL_RGBA_INTEGER};
			int i = (C <= 1) ? 0 : (C - 1);
			return isInteger() ? fmt_integer[i] : fmt_float[i];
		}


	};
}