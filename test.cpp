#include <windows.h>

#include "webview.h"

// This file is not included into the GIT repo
#include "settings.h"

webview_t mywv;

HWND mainhwnd;

void CreateWebView(HWND hwnd)
{
	memset(&mywv, 0, sizeof(webview_t));
	mywv.title = "Login to Facebook";
	mywv.url = SETTINGS_TEST_URL;
	mywv.width = 500;
	mywv.height = 500;
	mywv.resizable = true;
	mywv.parent = (void*)hwnd;
	mywv.oauth_callback_prefix = "https://www.facebook.com/connect/login_success.html";

	int r = webview_init(&mywv);
}


LRESULT WINAPI MyWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
	case WM_COMMAND:
		if (LOWORD(wparam) == 100) {
			CreateWebView(hwnd);
		}
		return 0;
	case WM_CLOSE:
		if (hwnd == mainhwnd)
			PostQuitMessage(0);
		return 1;
	}

	return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR cmdline, int show)
{
	WNDCLASSW cls;

	cls.cbClsExtra = 0;
	cls.cbWndExtra = 0;
	cls.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	cls.hCursor = LoadCursorA(NULL, IDC_ARROW);
	cls.hIcon = LoadIconA(NULL, IDI_APPLICATION);
	cls.hInstance = hinst;
	cls.lpfnWndProc = &MyWndProc;
	cls.lpszClassName = L"WebViewTestCls";
	cls.lpszMenuName = NULL;
	cls.style = CS_HREDRAW | CS_VREDRAW;

	RegisterClassW(&cls);

	auto hwnd = CreateWindowW(L"WebViewTestCls", L"WebView test", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hinst, 0);

	mainhwnd = hwnd;

	CreateWindowW(L"BUTTON", L"Click Me", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 10, 10, 120, 40, hwnd, (HMENU)100, hinst, NULL);

	ShowWindow(hwnd, SW_SHOW);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	//webview_exit(&mywv);

	return msg.wParam;
}