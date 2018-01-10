#pragma once

#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>

#define DESKTOP_USE_EGL

#include <string>
#include <HalideRuntimeOpenGL.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#elif defined(__ANDROID__) || defined(DESKTOP_USE_EGL)

#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#elif _WIN32
#include <Windows.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GL/gl.h>
#endif

#include "Stopwatch.h"

namespace egl {
	void *create_window();

	void destroy_window(void *handle);

	EGLContext create_context(void *window, bool debug);
}

#include <stdexcept>
#include <cstring>

#include <pclog/pclog.h>
#include <vector>

namespace gl {
	struct glSymbol {
		GLint value;
		char const *name;

		template<std::size_t N>
		constexpr glSymbol(GLint value, char const (&name)[N]) :value(value), name(name) {}
	};

	struct float16_t {
		uint16_t h;
	};

	struct buf_use {
		bool cpuRead, cpuWrite, gpuRead, gpuWrite;

		inline buf_use(bool cr, bool cw, bool gr, bool gw)
		: cpuRead(cr), cpuWrite(cw), gpuRead(gr), gpuWrite(gw){};

		static buf_use to_gpu() { return {false, true, true, false}; }

		static buf_use to_cpu() { return {true, false, false, true}; }

		static buf_use duplex() { return {true, true, true, true}; }

		bool _forceWidth = false;

		buf_use &forceWidth() {
			_forceWidth = true;
			return *this;
		}
	};



#define GL_SYM(NAME) glSymbol{ GL_ ## NAME, #NAME }
#define GL_SYM_SFMTS(T) {GL_SYM(R ## T), GL_SYM(RG ## T), GL_SYM(RGB ## T), GL_SYM(RGBA ## T)}

	static constexpr glSymbol
			sfmts_f32[] = GL_SYM_SFMTS(32F),
			sfmts_u32[] = GL_SYM_SFMTS(32UI), sfmts_i32[] = GL_SYM_SFMTS(32I),
			sfmts_u16[] = GL_SYM_SFMTS(16UI), sfmts_i16[] = GL_SYM_SFMTS(16I),
			sfmts_u8[] = GL_SYM_SFMTS(8UI), sfmts_i8[] = GL_SYM_SFMTS(8I);

	class context {
	public:
		void *volatile winHandle;
		EGLContext eglContext;
		EGLSurface eglSurface;
		EGLDisplay eglDisplay;
		bool isGLES;
		int verMajor, verMinor;

		// Check whether string starts with a given prefix.
		// Returns pointer to character after matched prefix if successful or NULL.
		static const char *match_prefix(const char *s, const char *prefix) {
			if (0 == strncmp(s, prefix, strlen(prefix))) {
				return s + strlen(prefix);
			}
			return NULL;
		}

		static const char *parse_int(const char *str, int *val) {
			int v = 0;
			size_t i = 0;
			while (str[i] >= '0' && str[i] <= '9') {
				v = 10 * v + (str[i] - '0');
				i++;
			}
			if (i > 0) {
				*val = v;
				return &str[i];
			}
			return NULL;
		}

		static const char *parse_opengl_version(const char *str, int *major, int *minor) {
			str = parse_int(str, major);
			if (str == NULL || *str != '.') {
				return NULL;
			}
			return parse_int(str + 1, minor);
		}

		static void debug_callback(GLenum source,
								   GLenum type,
								   GLuint id,
								   GLenum severity,
								   GLsizei length,
								   const GLchar *message,
								   const void *userParam) {
			// ignore notifications
			if (severity == GL_DEBUG_SEVERITY_NOTIFICATION_KHR)
				return;

			std::string msg(message, length);
			PCLogLevel ll = logDEBUG;

			//dont print shader sources
			if (severity == GL_DEBUG_SEVERITY_NOTIFICATION_KHR/*33387*/ && msg.size() > 200)
				return;

			if (severity == GL_DEBUG_SEVERITY_MEDIUM_KHR/*37191*/) {
				ll = logWARNING;
			} else if (severity != GL_DEBUG_SEVERITY_NOTIFICATION_KHR/*33387*/ &&
					   (type == GL_DEBUG_TYPE_ERROR_KHR /*33356*/ ||
						severity == GL_DEBUG_SEVERITY_HIGH_KHR)) {
				ll = logERROR;
			}

			LOG(ll) << msg << " (GL_DBG type=" << type << ",id=" << id << ",severity=" << severity
					<< ")";
		}

		bool debug;

		explicit context(bool debug) : debug(debug) {

		}

		~context() {
			//egl::destroy_window(winHandle); 
		}

		void initForThisThread() {
			static void *win = 0;
			if (win == 0) win = egl::create_window();
			//egl::create_window();
			winHandle = win; // egl::create_window();

			if (eglGetCurrentContext() != EGL_NO_CONTEXT)
				return;

			void *user_context = 0;
			if (debug) {
				//LOG_D << "creating EGL debug context...";
			}

			eglContext = egl::create_context(winHandle, debug);
			if (eglContext != eglGetCurrentContext()) {
				throw std::runtime_error("created context is not current");
			}

			eglDisplay = eglGetCurrentDisplay();
			eglSurface = eglGetCurrentSurface(EGL_DRAW);

			const char *version = (const char *) glGetString(GL_VERSION);
			checkAndThrowError("context(): glGetString");

			const char *gles_version = match_prefix(version, "OpenGL ES ");
			int major, minor;
			if (gles_version && parse_opengl_version(gles_version, &major, &minor)) {
				isGLES = true;
				verMajor = major;
				verMinor = minor;
			} else if (parse_opengl_version(version, &major, &minor)) {
				isGLES = false;
				verMajor = major;
				verMinor = minor;
			} else {
				std::runtime_error("unsupported OpenGL version 2.0");
			}

			static bool wasHere = false;
			if (!wasHere) {
				LOG_I << (isGLES ? "OpenGLES" : "OpenGL") << " version " << verMajor << "."
					  << verMinor;
				wasHere = true;
			}

//			glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, 1);
//			glDebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, 1);
//			glDebugMessageControl(GL_DEBUG_SOURCE_OTHER , GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, 1);
			if (debug) {
				//glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
				glDebugMessageCallbackKHR(&debug_callback, 0);
				checkAndThrowError("context(): glDebugMessageCallbackKHR");
			}
		}

		void deinitForThisThread() {
			// release the context from this thread
			eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
			//eglDestroySurface(eglDisplay, eglSurface);
			//eglDestroyContext(eglDisplay, eglContext);
		}

		static const char *gl_error_name(int32_t err) {
			const char *result;
			switch (err) {
				case 0x500:
					result = "GL_INVALID_ENUM";
					break;
				case 0x501:
					result = "GL_INVALID_VALUE";
					break;
				case 0x502:
					result = "GL_INVALID_OPERATION";
					break;
				case 0x503:
					result = "GL_STACK_OVERFLOW";
					break;
				case 0x504:
					result = "GL_STACK_UNDERFLOW";
					break;
				case 0x505:
					result = "GL_OUT_OF_MEMORY";
					break;
				case 0x506:
					result = "GL_INVALID_FRAMEBUFFER_OPERATION";
					break;
				case 0x507:
					result = "GL_CONTEXT_LOST";
					break;
				case 0x8031:
					result = "GL_TABLE_TOO_LARGE";
					break;
				default:
					result = "<unknown GL error>";
					break;
			}
			return result;
		}

		bool checkAndReportError(const char *location) {
			GLenum err = glGetError();
			if (err != GL_NO_ERROR) {
				LOG_E << "OpenGL error " << gl_error_name(err) << "(" << (int) err << ")" <<
					  " at " << location << ".\n";
				return true;
			}
			return false;
		}

		static void checkAndThrowError(const std::string &location) {
			GLenum err = glGetError();
			if (err != GL_NO_ERROR) {
				LOG_E << "OpenGL error " << gl_error_name(err) << "(" << (int) err << ")" <<
					  " at " << location << ".\n";
				throw std::runtime_error(
						"OpenGL error " + std::string(gl_error_name(err)) + " at " +
						std::string(location));
			}
		}


		template<class T>
		static void checkAndThrowError(const std::string &location, const T &obj) {
			GLenum err = glGetError();
			auto objStr = obj.to_string();
			if (err != GL_NO_ERROR) {
				LOG_E << "OpenGL error " << gl_error_name(err) << "(" << (int) err << ")" <<
					  " at " << location << " with " << objStr << "\n";
				throw std::runtime_error(
						"OpenGL error " + std::string(gl_error_name(err)) + " at " +
						std::string(location) + "with " + objStr);
			}
		}

		GLuint LoadShader(GLenum type, const char *shaderSrc) {
			GLuint shader;
			GLint compiled;

			// Create the shader object
			shader = glCreateShader(type);

			if (shader == 0) {
				return 0;
			}

			// Load the shader source
			glShaderSource(shader, 1, &shaderSrc, NULL);

			// Compile the shader
			glCompileShader(shader);

			// Check the compile status
			glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

			if (!compiled) {
				GLint infoLen = 0;

				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

				if (infoLen > 1) {
					char *infoLog = (char *) malloc(sizeof(char) * infoLen);
					glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
					LOG_E << "Error compiling shader:" << infoLog;
					free(infoLog);
				}

				glDeleteShader(shader);
				return 0;
			}

			return shader;

		}

		GLuint LoadShader(GLenum type, const std::string &shaderSrc) {
			return LoadShader(type, shaderSrc.c_str());
		}

		GLuint Link(std::vector<GLuint> shaders) {
			auto programObject = glCreateProgram();

			for (auto sh : shaders)
				glAttachShader(programObject, sh);

			glLinkProgram(programObject);
			checkAndReportError("gl::Link(): glLinkProgram");

			GLint linked = 0;
			glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

			if (!linked) {

				GLint infoLen = 0;

				glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);

				std::string istr;

				if (infoLen > 1) {
					char *infoLog = (char *) malloc(sizeof(char) * infoLen);
					glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);
					istr = "Error linking program: " + std::string(infoLog);
					free(infoLog);
				} else {
					istr = "Error linking program";
				}

				glDeleteProgram(programObject);

				LOG_E << istr;
				throw std::runtime_error(istr);
			}

			checkAndThrowError("gl::Link(): unexpected error state");

			return programObject;
		}

		bool makeCurrent() {
			return eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
		}
	};

	static std::string egl_error_name(int32_t err) {
		switch (err) {
#define caseReturn(S) case S: return # S
			caseReturn(EGL_BAD_DISPLAY);
			caseReturn(EGL_BAD_SURFACE);
			caseReturn(EGL_BAD_ATTRIBUTE);
			caseReturn(EGL_NOT_INITIALIZED);
			caseReturn(EGL_BAD_PARAMETER);
			caseReturn(EGL_BAD_MATCH);
			caseReturn(EGL_BAD_CONFIG);
			caseReturn(EGL_BAD_CONTEXT);
			caseReturn(EGL_BAD_NATIVE_WINDOW);
			caseReturn(EGL_BAD_ACCESS);
			caseReturn(EGL_BAD_ALLOC);
#undef caseReturn
			default:
				return "<unknown EGL error " + std::to_string(err) + ">";
		}
	}


	class glsl_processor {
		typedef std::string string;
		std::vector<std::pair<string, string>> macros;

		void addMacro(const string &name, const string &value) {
			macros.push_back({name, value});
		}

	public:
		template<typename T>
		glsl_processor &d(const string &name, T v) {
			addMacro(name, std::to_string(v));
			return *this;
		}

		string process(string src) {
			for (auto &p : macros) {
				auto &s(p.first), t(p.second);
				std::string::size_type n = 0;
				while ((n = src.find(s, n)) != std::string::npos) {
					src.replace(n, s.size(), t);
					n += t.size();
				}
			}
			LOG_D << src;
			return src;
		}
	};
}

namespace egl {
	static void checkAndThrowError(const std::string &location) {
		auto err = eglGetError();
		if (err != EGL_SUCCESS) {
			LOG_E << "EGL error " << gl::egl_error_name(err) << "(" << (int) err << ")" <<
				  " at " << location << ".\n";
			throw std::runtime_error(
					"EGL error at " + location + ": " + gl::egl_error_name(err));
		}
	}
}

