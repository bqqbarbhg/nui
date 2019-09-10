#include "nui_canvas_gdi.h"

static COLORREF to_colorref(nui_color col) {
	return RGB(col.r, col.g, col.b);
}

static RECT to_rect(nui_rect rect) {
	RECT rc = { rect.left, rect.top, rect.right, rect.bottom };
	return rc;
}

void nui_gdi_render(HDC dc, const nui_render_info *ri)
{
	nui_draw *ptr = nui_draws_begin(ri->layer);
	nui_draw *end = nui_draws_end(ri->layer);

	SetBkMode(dc, TRANSPARENT);

	for (; ptr != end; ptr = nui_next_draw(ptr)) {

		switch (ptr->type) {

		case nui_dt_rect: {
			nui_rect_draw *draw = (nui_rect_draw*)ptr;
			HBRUSH brush = CreateSolidBrush(to_colorref(draw->color));
			RECT rc = to_rect(draw->rect);
			FillRect(dc, &rc, brush);
			DeleteObject(brush);
		} break;

		case nui_dt_text: {
			nui_text_draw *draw = (nui_text_draw*)ptr;
			SetTextColor(dc, to_colorref(draw->color));
			TextOutA(dc, draw->pos.x, draw->pos.y, draw->text, (int)draw->text_len);
		} break;

		case nui_dt_layer: {
			nui_layer_draw *draw = (nui_layer_draw*)ptr;

			nui_render_info lri;
			lri.layer = draw->layer;
			lri.offset.x = ri->offset.x + draw->pos.x;
			lri.offset.y = ri->offset.y + draw->pos.y;
			nui_gdi_render(dc, &lri);
		} break;

		}
	}
}
