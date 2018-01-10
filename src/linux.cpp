
#include <cstdint>

#define GL_GLEXT_PROTOTYPES

#include <GLES3/gl3.h>


#include <pclog/pclog.h>
#include <thread>

extern "C" {
#define EGLAPI
#define EGLAPIENTRY

typedef int32_t EGLint;
typedef unsigned int EGLBoolean;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLNativeDisplayType;
typedef void *EGLConfig;
typedef void *EGLSurface;
#define EGL_NO_CONTEXT                  ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY             ((EGLNativeDisplayType)0)
#define EGL_NO_DISPLAY                  ((EGLDisplay)0)
#define EGL_NO_SURFACE                  ((EGLSurface)0)

#define EGL_ALPHA_SIZE                  0x3021
#define EGL_BLUE_SIZE                   0x3022
#define EGL_GREEN_SIZE                  0x3023
#define EGL_RED_SIZE                    0x3024
#define EGL_SURFACE_TYPE                0x3033
#define EGL_NONE                        0x3038
#define EGL_RENDERABLE_TYPE             0x3040
#define EGL_OPENGL_ES3_BIT_KHR            0x00000040
#define EGL_HEIGHT                      0x3056
#define EGL_WIDTH                       0x3057
#define EGL_CONTEXT_CLIENT_VERSION      0x3098

#define EGL_CONTEXT_MAJOR_VERSION    0x3098
#define EGL_CONTEXT_MINOR_VERSION        0x30FB
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#define EGL_CONTEXT_FLAGS_KHR             0x30FC

#define EGL_PBUFFER_BIT                 0x0001
#define EGL_OPENGL_ES2_BIT              0x0004

EGLAPI EGLint EGLAPIENTRY eglGetError(void);
EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext(void);
EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id);
EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
											  EGLConfig *configs, EGLint config_size,
											  EGLint *num_config);
EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config,
											   EGLContext share_context,
											   const EGLint *attrib_list);
EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
													  const EGLint *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
											 EGLSurface read, EGLContext ctx);

EGLAPI void *eglGetProcAddress(const char *procname);

EGLAPI void setGLDebugLevel(int level);

extern int strcmp(const char *, const char *);

}


namespace egl {
	void *create_window() {return 0;}

	EGLContext create_context(void *window, bool debug) {
		EGLContext context = eglGetCurrentContext();
		if (context != EGL_NO_CONTEXT) {
			LOG_D << "Re-using current EGL context";
			return context;
		}

		EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (display == EGL_NO_DISPLAY || !eglInitialize(display, 0, 0)) {
			throw std::runtime_error("Could not initialize EGL display: " + std::to_string(eglGetError()));
		}

		EGLint attribs[] = {
				EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_ALPHA_SIZE, 8,
				EGL_NONE,
		};
		EGLConfig config;
		int numconfig;
		eglChooseConfig(display, attribs, &config, 1, &numconfig);
		if (numconfig != 1) {
			throw std::runtime_error("eglChooseConfig(): config not found: " + std::to_string(eglGetError()) + " - " + std::to_string(numconfig));
		}

		EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
									EGL_CONTEXT_MAJOR_VERSION, 3,
									EGL_CONTEXT_MINOR_VERSION, 1,
									EGL_CONTEXT_FLAGS_KHR,
									debug ? EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0,
									EGL_NONE};

		context = eglCreateContext(display, config, EGL_NO_CONTEXT,
											  context_attribs);
		if (context == EGL_NO_CONTEXT) {
			throw std::runtime_error("Error: eglCreateContext failed: " +  std::to_string(eglGetError()));
		}

		EGLint surface_attribs[] = {
				EGL_WIDTH, 1,
				EGL_HEIGHT, 1,
				EGL_NONE
		};
		EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attribs);
		if (surface == EGL_NO_SURFACE) {
			throw std::runtime_error("Error: Could not create EGL window surface: "  +  std::to_string(eglGetError()));
		}

		eglMakeCurrent(display, surface, surface, context);
		//LOG_D << "made EGL context current for thread " <<  std::this_thread::get_id();

		static bool wasDebug = false;
		if (debug && !wasDebug) {
			LOG_W << "Created EGL _DEBUG_ Context";
			wasDebug = true;
		}

		return context;
	}
}