#include "context.h"

#include "egl.h"
#include "egl_win.h"

inline std::string lastError(int err = 0) {
	err = err ? err : GetLastError();

	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();

	FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL
	);

	std::string msg((LPTSTR) lpMsgBuf);
	LocalFree(lpMsgBuf);

	msg += " (" + std::to_string(dw) + ")";

	return msg;
}


LRESULT CALLBACK win_event(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	return DefWindowProc(hwnd, msg, wparam, lparam);
}


EGLContext egl::create_context(void *window, bool debug) {
#ifndef DESKTOP_USE_EGL
	HMODULE mod = GetModuleHandle(0);

	WNDCLASS wndClass;
	wndClass.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hInstance = mod;
	wndClass.lpszMenuName = NULL;
	wndClass.lpfnWndProc = win_event;
	wndClass.lpszClassName = "ogl";
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.style = CS_HREDRAW | CS_VREDRAW;

	int err = RegisterClass(&wndClass);
	if (err < 0) {
		MessageBox(NULL, "Can not register window class!", "Error", 0);
		return 0;
	}
	HWND hwnd = CreateWindow("ogl", "hidden", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
							 CW_USEDEFAULT, 0, 0, mod, 0);

	if (!hwnd) {
		printf("%s\n", lastError().c_str());
		return 0;
	}
	ShowWindow(hwnd, SW_HIDE);
	static HGLRC hRC;    //rendering context
	static HDC hDC;    //device context
	hDC = GetDC(hwnd);    //get the device context for window


	PIXELFORMATDESCRIPTOR pfd = {
			sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
			24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
	};

	GLint iPixelFormat;

	if ((iPixelFormat = ChoosePixelFormat(hDC, &pfd)) == 0) {
		MessageBox(NULL, "ChoosePixelFormat Failed", "Error", 0);
		return 0;
	}
	if (SetPixelFormat(hDC, iPixelFormat, &pfd) == FALSE) {
		MessageBox(NULL, "SetPixelFormat Failed", "Error", 0);
		return 0;
	}


	hRC = wglCreateContext(hDC);    //create rendering context
	if (hRC == 0) {
		printf("%s\n", lastError().c_str());
		return 0;
	}
	wglMakeCurrent(hDC, hRC);    //make rendering context current
#else

	return egl::createContext((HWND)window);
#endif
}

void *egl::create_window() {
	HWND winHandle = 0;

	//static HWND winHandle = 0;
	//if (winHandle)
	//	return winHandle;

	
	DWORD wStyle = 0;
	RECT windowRect;
	HINSTANCE hInstance = GetModuleHandle(NULL);


	static bool first = true;
	if (first) {
		first = false;

		WNDCLASS wndclass = { 0 };
		wndclass.style = CS_OWNDC;
		wndclass.lpfnWndProc = (WNDPROC)win_event;
		wndclass.hInstance = hInstance;
		wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wndclass.lpszClassName = "opengles3.0";

		if (!RegisterClass(&wndclass)) {
			return 0;
		}
	}

	wStyle = WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION;

	// Adjust the window rectangle so that the client area has
	// the correct number of pixels
	windowRect.left = 0;
	windowRect.top = 0;
	windowRect.right = 320; // width
	windowRect.bottom = 240; // height

	AdjustWindowRect(&windowRect, wStyle, FALSE);


	winHandle = CreateWindow(
		"opengles3.0", "gl", wStyle, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		NULL, NULL, hInstance, NULL);


	if (winHandle == NULL) {
		return 0;
	}

	ShowWindow(winHandle, FALSE);
	return (void*)winHandle;
}

void egl::destroy_window(void *handle) {
	DestroyWindow((HWND)handle);
}

extern "C" void *halide_opengl_get_proc_address(void *, const char *name) {
#ifndef DESKTOP_USE_EGL
	void *p = (void *) wglGetProcAddress(name);
	if (p == 0 ||
		(p == (void *) 0x1) || (p == (void *) 0x2) || (p == (void *) 0x3) ||
		(p == (void *) -1)) {
		HMODULE module = LoadLibraryA("opengl32.dll");
		p = (void *) GetProcAddress(module, name);
	}

	return p;
#else
	return (void *) eglGetProcAddress(name);
#endif
}

extern "C" int halide_opengl_create_context(void *) {
	auto win = egl::create_window();
	bool res = egl::create_context(win, true);
	LOG_I << "halide_opengl_create_context: res="<<res;
	return res ? 0 : 1;
}



