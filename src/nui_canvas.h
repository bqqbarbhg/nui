#pragma once

#include "nui_base.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nui_canvas nui_canvas;
typedef struct nui_layer nui_layer;

typedef enum nui_draw_type {
	nui_dt_rect,
	nui_dt_text,
	nui_dt_layer,
} nui_draw_type;

typedef struct nui_draw {
	nui_draw_type type;
	uint32_t size;
} nui_draw;

typedef struct nui_rect_draw {
	nui_draw draw;
	nui_rect rect;
	nui_color color;
} nui_rect_draw;

typedef struct nui_text_draw {
	nui_draw draw;
	nui_point pos;
	nui_color color;
	uint32_t text_len;
	char text[1];
} nui_text_draw;

typedef struct nui_layer_draw {
	nui_draw draw;
	nui_point pos;
	nui_layer *layer;
} nui_layer_draw;

typedef struct nui_render_info {
	nui_layer *layer;
	nui_rect clip;
	nui_point offset;
} nui_render_info;

// nui_canvas

nui_canvas *nui_make_canvas();
void nui_free_canvas(nui_canvas *c);

// nui_layer

nui_layer *nui_make_layer(nui_canvas *c, int32_t x, int32_t y);
void nui_free_layer(nui_layer *l);

nui_extent nui_layer_size(const nui_layer *l);
void nui_resize_layer(nui_layer *l, int32_t x, int32_t y);
static void nui_resize_layer_ex(nui_layer *l, nui_extent size) {
	nui_resize_layer(l, size.x, size.y);
}

uint32_t nui_layer_index(const nui_layer *l);

// Drawing

void nui_clear(nui_layer *l);
void nui_fill_rect(nui_layer *l, const nui_rect *r, nui_color color);
void nui_draw_text_len(nui_layer *l, nui_point pos, nui_color color, const char *text, size_t len);
static void nui_draw_text(nui_layer *l, nui_point pos, nui_color color, const char *text) {
	nui_draw_text_len(l, pos, color, text, strlen(text));
}
void nui_draw_layer(nui_layer *l, nui_point pos, nui_layer *inner);

// Rendering

void nui_begin_rendering(nui_canvas *c);
void nui_end_rendering(nui_canvas *c);

int nui_layer_dirty(nui_layer *l);

nui_draw *nui_draws_begin(nui_layer *l);
nui_draw *nui_draws_end(nui_layer *l);
static nui_draw *nui_next_draw(nui_draw *d) {
	return (nui_draw*)((char*)d + d->size);
}


#ifdef __cplusplus
}
#endif
