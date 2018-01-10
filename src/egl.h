#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace egl {

	static std::string egl_error_name(int32_t err) {
		switch (err) {
			case EGL_BAD_DISPLAY :
				return  "EGL_BAD_DISPLAY";
			case EGL_BAD_ATTRIBUTE:
				return  "EGL_BAD_ATTRIBUTE";
			case EGL_NOT_INITIALIZED:
				return  "EGL_NOT_INITIALIZED";
			case EGL_BAD_PARAMETER:
				return  "EGL_BAD_PARAMETER";
			case EGL_BAD_MATCH:
				return  "EGL_BAD_MATCH";
			case EGL_BAD_CONFIG:
				return  "EGL_BAD_CONFIG";
			case EGL_BAD_CONTEXT:
				return  "EGL_BAD_CONTEXT";
			default:
				return "<unknown EGL error " + std::to_string(err) + ">";
		}
	}


	EGLint GetContextRenderableType ( EGLDisplay eglDisplay )
	{
#ifdef EGL_KHR_create_context
		const char *extensions = eglQueryString ( eglDisplay, EGL_EXTENSIONS );

		// check whether EGL_KHR_create_context is in the extension string
		if ( extensions != NULL && strstr( extensions, "EGL_KHR_create_context" ) )
		{
			// extension is supported
			return EGL_OPENGL_ES3_BIT_KHR;
		}
#endif
		// extension is not supported
		return EGL_OPENGL_ES2_BIT;
	}


	EGLContext createContext(HWND winHandle) {
		static HWND lastWin = 0;
		static EGLSurface lastSurf;
		static EGLContext lastContext;

		EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

		if (lastWin == winHandle) {
			eglMakeCurrent(display, lastSurf, lastSurf, lastContext);
			checkAndThrowError("eglMakeCurrent");
			return lastContext;
		}

	
		EGLint majorVersion;
		EGLint minorVersion;

		eglInitialize(display, &majorVersion, &minorVersion);
		checkAndThrowError("eglInitialize");

		//LOG_I << "Initialized EGL v"<< majorVersion << "." <<minorVersion;

		EGLint attribList[] =
				{
						EGL_RED_SIZE,       5,
						EGL_GREEN_SIZE,     6,
						EGL_BLUE_SIZE,      5,
						EGL_ALPHA_SIZE,     EGL_DONT_CARE,
						EGL_DEPTH_SIZE,     EGL_DONT_CARE,
						EGL_STENCIL_SIZE,   EGL_DONT_CARE,
						EGL_SAMPLE_BUFFERS, 0,
						// if EGL_KHR_create_context extension is supported, then we will use
						// EGL_OPENGL_ES3_BIT_KHR instead of EGL_OPENGL_ES2_BIT in the attribute list
						EGL_RENDERABLE_TYPE, GetContextRenderableType ( display ),
						EGL_NONE
				};

		EGLConfig config;
		EGLint num_configs;
		eglChooseConfig(display, attribList, &config, 1, &num_configs);
		checkAndThrowError("eglChooseConfig");

		if ( num_configs < 1 )
		{
			throw std::runtime_error("no configs");
		}

		EGLSurface eglSurface = eglCreateWindowSurface(display, config, winHandle, NULL);
		checkAndThrowError("eglCreateWindowSurface with window H" + std::to_string((uint64_t)winHandle));
		lastWin = winHandle;

		



		bool debug = true;

			EGLContext context;
		EGLint context_attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
									   EGL_CONTEXT_MINOR_VERSION, 1,
									   EGL_CONTEXT_FLAGS_KHR, debug ? EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR : 0,
									   EGL_NONE};

		context = eglCreateContext(display, config, EGL_NO_CONTEXT,
								   context_attributes);
		checkAndThrowError("eglCreateContext");



		if (debug) {
			LOG_W << "OpenGL in DEBUG mode!";
		}


		eglMakeCurrent(display, eglSurface, eglSurface, context);
		checkAndThrowError("eglMakeCurrent");


		lastSurf = eglSurface;
		lastContext = context;

		return context;
	}
};