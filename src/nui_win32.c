#include "nui.h"
#include "nui_backend.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

typedef struct {
	nui_context *nui_ctx;

	HINSTANCE instance;
	ATOM class_atom;
} nui_win32_context;

typedef struct {
	nui_win32_context *context;
	nui_window *nui_win;
	HWND hwnd;
} nui_win32_window;

typedef struct {
	nui_extent offset;
	nui_bounds clip;
} nui_win32_draw_params;

static COLORREF to_colorref(nui_color color)
{
	return RGB(color.r, color.g, color.b);
}

static void nui_win32_draw_layer(nui_win32_context *ctx, HDC dc, nui_layer *layer, const nui_win32_draw_params *params)
{
	nui_draw *draw_ptr = nui_be_draws_begin(layer);
	nui_draw *end_draw = nui_be_draws_end(layer);
	nui_extent offset = params->offset;

	for (; draw_ptr != end_draw; draw_ptr = nui_be_next_draw(draw_ptr)) {
		if (!nui_intersects(draw_ptr->bounds, params->clip)) continue;
		RECT rc;
		rc.left   = draw_ptr->bounds.min.x + offset.x;
		rc.top    = draw_ptr->bounds.min.y + offset.y;
		rc.right  = draw_ptr->bounds.max.x + offset.x;
		rc.bottom = draw_ptr->bounds.max.y + offset.y;

		switch (draw_ptr->type) {

		case nui_dt_rect: {
			nui_rect_draw *draw = (nui_rect_draw*)draw_ptr;
			HBRUSH brush = CreateSolidBrush(to_colorref(draw->color));
			FillRect(dc, &rc, brush);
			DeleteObject(brush);
		} break;

		case nui_dt_text: {
			nui_text_draw *draw = (nui_text_draw*)draw_ptr;
			LONG flags = DT_LEFT | DT_TOP | DT_NOPREFIX;
			SetTextColor(dc, to_colorref(draw->color));
			DrawTextA(dc, draw->text, (int)draw->text_len, &rc, flags);
		} break;

		case nui_dt_layer: {
			nui_layer_draw *draw = (nui_layer_draw*)draw_ptr;

			int32_t width = draw->draw.bounds.max.x - draw->draw.bounds.min.x;
			int32_t height = draw->draw.bounds.max.y - draw->draw.bounds.min.y;

			nui_win32_draw_params layer_params = { 0 };
			layer_params.offset.x = rc.left;
			layer_params.offset.y = rc.top;
			layer_params.clip.min.x = nui_max(draw->offset.x, params->clip.min.x);
			layer_params.clip.min.y = nui_max(draw->offset.y, params->clip.min.y);;
			layer_params.clip.max.x = nui_min(width + draw->offset.x, params->clip.max.x);
			layer_params.clip.max.y = nui_min(height + draw->offset.y, params->clip.max.y);
			if (layer_params.clip.min.x < layer_params.clip.max.x
				&& layer_params.clip.min.y < layer_params.clip.max.y) {
				nui_win32_draw_layer(ctx, dc, draw->layer, &layer_params);
			}

		} break;

		}
	}
}

static LRESULT CALLBACK nui_win32_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG_PTR user = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (!user) {
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
	nui_win32_window *window = (nui_win32_window*)user;
	nui_win32_context *ctx = window->context;
	
	switch (uMsg) {

	case WM_SIZE: {
		int32_t width = LOWORD(lParam);
		int32_t height = HIWORD(lParam);
		nui_resize_container(nui_window_container(window->nui_win), width, height);
	} break;
	
	case WM_PAINT: {
		nui_render_layers(ctx->nui_ctx);

		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hwnd, &ps);
		nui_layer *layer = nui_be_window_layer(window->nui_win);

		// TEMP
		HBRUSH clear_brush = CreateSolidBrush(0xed9564);
		FillRect(dc, &ps.rcPaint, clear_brush);
		DeleteObject(clear_brush);

		if (layer) {
			nui_win32_draw_params params = { 0 };
			params.clip.min.x = ps.rcPaint.left;
			params.clip.min.y = ps.rcPaint.top;
			params.clip.max.x = ps.rcPaint.right;
			params.clip.max.y = ps.rcPaint.bottom;
			nui_win32_draw_layer(ctx, dc, layer, &params);
		}
		EndPaint(hwnd, &ps);

		return 1;
	} break;

	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

static const nui_window_desc nui_win32_default_desc = {
	800, 600,
};

static void *nui_win32_make_window(void *user, nui_window *nui_win, const nui_window_desc *desc)
{
	if (!desc) desc = &nui_win32_default_desc;

	nui_win32_context *ctx = (nui_win32_context*)user;

	nui_win32_window *window = (nui_win32_window*)nui_alloc(sizeof(nui_win32_window));
	window->context = ctx;
	window->nui_win = nui_win;

	DWORD style = WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME;
	DWORD ex_style = 0;

	window->hwnd = CreateWindowExW(
		ex_style, (LPWSTR)MAKEINTATOM(ctx->class_atom), L"NUI Window", style,
		CW_USEDEFAULT, CW_USEDEFAULT, (int)desc->width, (int)desc->height,
		NULL, NULL, ctx->instance, NULL);
	SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, (LONG_PTR)window);

	RECT rect;
	GetClientRect(window->hwnd, &rect);
	int32_t width = rect.right - rect.left;
	int32_t height = rect.bottom - rect.top;
	nui_resize_container(nui_window_container(window->nui_win), width, height);

	return window;
}

static int nui_win32_event_loop(void *user, uint64_t timeout_ns)
{
	MSG msg;
	if (timeout_ns == NUI_TIMEOUT_INFINITE) {
		BOOL ret = GetMessageW(&msg, NULL, 0, 0);
		if (!ret) return 0;
	} else {
		BOOL ret = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
		if (!ret) {
			if (timeout_ns == 0) return 0;
			uint64_t wait_ms = (uint64_t)(timeout_ns / UINT64_C(1000000));
			if (wait_ms > INT32_MAX) wait_ms = INT32_MAX;
			MsgWaitForMultipleObjects(0, NULL, FALSE, (DWORD)wait_ms, QS_ALLINPUT);
			ret = PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE);
			if (!ret) return 0;
		}
	}

	TranslateMessage(&msg);
	DispatchMessageW(&msg);
	return 1;
}

static void nui_win32_debug_print(void *user, const char *str, size_t length)
{
	OutputDebugStringA(str);
}

nui_context *nui_win32_make_context()
{
	nui_win32_context *ctx = (nui_win32_context*)nui_alloc(sizeof(nui_win32_context));
	ctx->instance = GetModuleHandle(NULL);

	WNDCLASSEXW wnd_class = { sizeof(WNDCLASSEXW) };
	wnd_class.style = CS_HREDRAW | CS_VREDRAW;
	wnd_class.hInstance = ctx->instance;
	wnd_class.lpszClassName = L"ui_lib_win32";
	wnd_class.lpfnWndProc = &nui_win32_wndproc;
	ctx->class_atom = RegisterClassExW(&wnd_class);

	nui_backend be;
	be.make_window = &nui_win32_make_window;
	be.event_loop = &nui_win32_event_loop;
	be.debug_print = &nui_win32_debug_print;
	nui_context *nui_ctx = nui_be_make_context(&be, ctx);
	ctx->nui_ctx = nui_ctx;
	return nui_ctx;
}
