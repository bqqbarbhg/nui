#include "nui_canvas.h"
#include "nui_renderer_gdi.h"
#include <time.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

nui_canvas *canvas;
nui_layer *layer;
nui_layer *inner;
nui_font *font;
HWND hwnd;

void test_init()
{
	canvas = nui_make_canvas(nui_gdi_renderer_make(GetDC(hwnd)));
	layer = nui_make_layer(canvas, nui_ex(0, 0));
	inner = nui_make_layer(canvas, nui_ex(290, 40));

	nui_set_bg_color(inner, nui_rgb(0x6495ed));

	nui_font_desc desc = { };
	desc.family = "Arial";
	desc.height = 40;
	font = nui_make_font(canvas, &desc);
}

void test_render()
{
	if (!canvas) return;

	time_t t = time(NULL);
	char tick[128];
	snprintf(tick, sizeof(tick), "Time: %u", (uint32_t)t);

	nui_extent ts = nui_measure(font, tick);

	RECT wrc;
	GetClientRect(hwnd, &wrc);

	nui_extent ls = nui_ex(wrc.right - wrc.left, wrc.bottom - wrc.top);
	nui_resize_layer(layer, ls);

	nui_point pos;
	pos.x = ls.x/2 - 290/2;
	pos.y = ls.y/2 - 40/2;

	nui_clear(layer);
	nui_draw_layer(layer, pos, inner);

	nui_clear(inner);
	nui_draw_text(inner, nui_pt(0, 0), font, nui_rgb(0), tick);

	nui_begin_rendering(canvas);
	if (nui_layer_invalidation(layer) != nui_inv_none) {
		InvalidateRect(hwnd, NULL, FALSE);
	}
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {

	case WM_CLOSE:
		PostMessageW(hwnd, WM_QUIT, 0, 0);
		break;

	case WM_SIZE: {
		test_render();
	} break;

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);

		nui_begin_rendering(canvas);

		nui_render_info ri = { layer };
		ri.clip.left = ps.rcPaint.left;
		ri.clip.top = ps.rcPaint.top;
		ri.clip.right = ps.rcPaint.right;
		ri.clip.bottom = ps.rcPaint.bottom;
		ri.bg_color = nui_rgb(0xffffff);
		nui_gdi_render(dc, &ri);

		nui_end_rendering(canvas);

		EndPaint(hwnd, &ps);

		return 1;
	} break;

	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PTSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEXW wnd_class = { sizeof(WNDCLASSEXW) };
	wnd_class.style = CS_HREDRAW | CS_VREDRAW;
	wnd_class.hInstance = hInstance;
	wnd_class.lpszClassName = L"ui_lib_win32";
	wnd_class.lpfnWndProc = &WndProc;
	ATOM class_atom = RegisterClassExW(&wnd_class);

	DWORD style = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
	DWORD ex_style = 0;

	hwnd = CreateWindowExW(
		ex_style, (LPWSTR)MAKEINTATOM(class_atom), L"NUI Window", style,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		NULL, NULL, hInstance, NULL);

	test_init();

	for (;;) {
		MSG msg;
		MsgWaitForMultipleObjects(0, NULL, FALSE, 10, QS_ALLINPUT);
		BOOL ret = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
		if (ret) {
			if (msg.message == WM_QUIT) break;
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		test_render();
	}

	return 0;
}