#include "nui_renderer_gdi.h"
#include "nui_canvas.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

typedef struct nui_gdi_font {
	HFONT font;
} nui_gdi_font;

typedef struct nui_gdi_renderer {
	nui_renderer r;

	HDC measure_dc;

	nui_gdi_font *fonts;
	uint32_t cap_fonts;

} nui_gdi_renderer;

static COLORREF to_colorref(nui_color col) {
	return RGB(col.r, col.g, col.b);
}

static RECT to_rect(nui_rect rect, nui_point off) {
	RECT rc = {
		rect.left + off.x,
		rect.top + off.y,
		rect.right + off.x,
		rect.bottom + off.y
	};
	return rc;
}

static WCHAR *to_wchar(WCHAR *local, size_t local_len, const char *str, size_t len, DWORD *p_wide_len)
{
	if (len == 0) {
		*p_wide_len = 0;
		local[0] = 0;
		return local;
	}

	if (len < local_len - 1) {
		DWORD wide_len = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, local, local_len);
		if (wide_len > 0) {
			*p_wide_len = wide_len;
			local[wide_len] = 0;
			return local;
		}
	}

	DWORD wide_len = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0);
	*p_wide_len = wide_len;
	WCHAR *buf = nui_alloc((wide_len + 1) * sizeof(WCHAR));
	MultiByteToWideChar(CP_UTF8, 0, str, (int)len, buf, wide_len);
	buf[wide_len] = 0;
	return buf;
}

static void free_wchar(WCHAR *local, WCHAR *str)
{
	if (str != local) {
		nui_free(str);
	}
}

static void nui_gdi_make_font(nui_renderer *nr, uint32_t font, const nui_font_desc *desc)
{
	nui_gdi_renderer *r = (nui_gdi_renderer*)nr;

	nui_buf_grow(&r->fonts, &r->cap_fonts, font + 1);
	nui_gdi_font *f = &r->fonts[font];
	if (f->font != NULL) {
		DeleteObject(f->font);
	}

	DWORD wlen;
	WCHAR wlocal[512];
	WCHAR *wfamily = to_wchar(wlocal, nui_arraysize(wlocal), desc->family, strlen(desc->family), &wlen);

	f->font = CreateFontW(
		(int)desc->height, 0, 0, 0, 0,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, FF_DONTCARE | DEFAULT_PITCH,
		wfamily);

	free_wchar(wlocal, wfamily);
}

static nui_extent nui_gdi_measure(nui_renderer *nr, uint32_t font, const char *str, size_t len)
{
	nui_gdi_renderer *r = (nui_gdi_renderer*)nr;

	DWORD wlen;
	WCHAR wlocal[512];
	WCHAR *wstr = to_wchar(wlocal, nui_arraysize(wlocal), str, len, &wlen);

	SIZE sz;
	SelectObject(r->measure_dc, r->fonts[font].font);
	GetTextExtentPoint32W(r->measure_dc, wstr, (int)wlen, &sz);

	free_wchar(wlocal, wstr);

	return nui_ex(sz.cx, sz.cy);
}

static void nui_gdi_free(nui_renderer *nr)
{
	nui_gdi_renderer *r = (nui_gdi_renderer*)nr;
	for (uint32_t i = 0; i < r->cap_fonts; i++) {
		nui_gdi_font *f = &r->fonts[i];
		if (f->font != NULL) {
			DeleteObject(f->font);
		}
	}
}

nui_renderer *nui_gdi_renderer_make(void *dc)
{
	nui_gdi_renderer *r = nui_make(nui_gdi_renderer);
	r->r.make_font = &nui_gdi_make_font;
	r->r.measure = &nui_gdi_measure;
	r->r.free = &nui_gdi_free;

	r->measure_dc = CreateCompatibleDC((HDC)dc);

	return &r->r;
}

static void render(nui_gdi_renderer *r, HDC dc, const nui_render_info *ri, int redraw)
{
	nui_draw *ptr = nui_draws_begin(ri->layer);
	nui_draw *end = nui_draws_end(ri->layer);

	SetBkMode(dc, TRANSPARENT);

	DWORD wlen;
	WCHAR wlocal[512];

	nui_color bg = nui_blend_over(ri->bg_color, nui_layer_bg_color(ri->layer));
	if (nui_layer_invalidation(ri->layer) >= nui_inv_self) {
		redraw = 1;
	}

	if (redraw) {
		RECT rc = to_rect(ri->clip, ri->offset);
		HBRUSH brush = CreateSolidBrush(to_colorref(bg));
		FillRect(dc, &rc, brush);
		DeleteObject(brush);
	}

	for (; ptr != end; ptr = nui_next_draw(ptr)) {
		if (!nui_intersects(&ptr->bounds, &ri->clip)) continue;

		switch (ptr->type) {

		case nui_dt_rect: if (redraw) {
			nui_rect_draw *draw = (nui_rect_draw*)ptr;
			HBRUSH brush = CreateSolidBrush(to_colorref(draw->color));
			RECT rc = to_rect(draw->draw.bounds, ri->offset);
			FillRect(dc, &rc, brush);
			DeleteObject(brush);
		} break;

		case nui_dt_text: if (redraw) {
			nui_text_draw *draw = (nui_text_draw*)ptr;
			nui_point p = nui_offset(draw->draw.bounds.min, ri->offset);

			WCHAR *wstr = to_wchar(wlocal, nui_arraysize(wlocal), draw->text, draw->text_len, &wlen);

			SelectObject(dc, r->fonts[draw->font].font);
			SetTextColor(dc, to_colorref(draw->color));
			TextOutW(dc, p.x, p.y, wstr, (int)wlen);

			free_wchar(wlocal, wstr);
		} break;

		case nui_dt_layer: {
			nui_layer_draw *draw = (nui_layer_draw*)ptr;
			nui_point p = nui_offset(draw->draw.bounds.min, ri->offset);
			nui_extent size = nui_layer_size(draw->layer);

			nui_render_info lri;
			lri.layer = draw->layer;
			lri.offset.x = ri->offset.x + p.x;
			lri.offset.y = ri->offset.y + p.y;
			lri.clip.min.x = nui_max(ri->clip.min.x - p.x, 0);
			lri.clip.min.y = nui_max(ri->clip.min.y - p.y, 0);
			lri.clip.max.x = nui_min(ri->clip.max.x - p.x, size.x);
			lri.clip.max.y = nui_min(ri->clip.max.y - p.y, size.y);
			render(r, dc, &lri, redraw);
		} break;

		}
	}
}

void nui_gdi_render(void *dc, const nui_render_info *ri)
{
	HDC hdc = (HDC)dc;
	nui_gdi_renderer *r = (nui_gdi_renderer*)nui_layer_renderer(ri->layer);
	render(r, hdc, ri, 0);
}
