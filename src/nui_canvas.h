#pragma once

#include "nui_base.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nui_renderer nui_renderer;
typedef struct nui_canvas nui_canvas;
typedef struct nui_layer nui_layer;
typedef struct nui_font nui_font;

typedef enum nui_draw_type {
	nui_dt_rect,
	nui_dt_text,
	nui_dt_layer,
} nui_draw_type;

typedef struct nui_draw {
	nui_draw_type type;
	uint32_t size;
	nui_rect bounds;
} nui_draw;

typedef struct nui_rect_draw {
	nui_draw draw;
	nui_color color;
} nui_rect_draw;

typedef struct nui_text_draw {
	nui_draw draw;
	uint32_t font;
	nui_color color;
	uint32_t text_len;
	char text[1];
} nui_text_draw;

typedef struct nui_layer_draw {
	nui_draw draw;
	nui_layer *layer;
} nui_layer_draw;

typedef struct nui_render_info {
	nui_layer *layer;
	nui_rect clip;
	nui_point offset;
	nui_color bg_color;
} nui_render_info;

typedef struct nui_font_desc {
	const char *family;
	int32_t height;
} nui_font_desc;

struct nui_renderer {
	void (*make_font)(nui_renderer *r, uint32_t font, const nui_font_desc *desc);
	nui_extent (*measure)(nui_renderer *r, uint32_t font, const char *str, size_t len);
	void (*free)(nui_renderer *r);
};

typedef enum nui_invalidation {
	nui_inv_none,   // Everything is valid
	nui_inv_child,  // Child has updated inside clip area
	nui_inv_self,   // Need to re-render self
	nui_inv_resize, // Children have been resized
} nui_invalidation;

// nui_canvas

nui_canvas *nui_make_canvas(nui_renderer *renderer);
void nui_free_canvas(nui_canvas *c);

// nui_layer

nui_layer *nui_make_layer(nui_canvas *c, nui_extent size);
void nui_free_layer(nui_layer *l);

nui_extent nui_layer_size(const nui_layer *l);
void nui_resize_layer(nui_layer *l, nui_extent size);
void nui_set_bg_color(nui_layer *l, nui_color color);

uint32_t nui_layer_index(const nui_layer *l);
nui_color nui_layer_bg_color(const nui_layer *l);

nui_canvas *nui_layer_canvas(const nui_layer *l);
nui_renderer *nui_layer_renderer(const nui_layer *l);

// nui_font

nui_font *nui_make_font(nui_canvas *c, const nui_font_desc *desc);
void nui_free_font(nui_font *font);

nui_extent nui_measure_len(nui_font *font, const char *text, size_t len);
static nui_extent nui_measure(nui_font *font, const char *text) {
	return nui_measure_len(font, text, strlen(text));
}

// Drawing

void nui_clear(nui_layer *l);
void nui_fill_rect(nui_layer *l, const nui_rect *r, nui_color color);
void nui_draw_text_len(nui_layer *l, nui_point pos, nui_font *font, nui_color color, const char *text, size_t len);
static void nui_draw_text(nui_layer *l, nui_point pos, nui_font *font, nui_color color, const char *text) {
	nui_draw_text_len(l, pos, font, color, text, strlen(text));
}
void nui_draw_layer(nui_layer *l, nui_point pos, nui_layer *inner);

// Rendering

void nui_begin_rendering(nui_canvas *c);
void nui_end_rendering(nui_canvas *c);

nui_invalidation nui_layer_invalidation(nui_layer *l);

nui_draw *nui_draws_begin(nui_layer *l);
nui_draw *nui_draws_end(nui_layer *l);
static nui_draw *nui_next_draw(nui_draw *d) {
	return (nui_draw*)((char*)d + d->size);
}


#ifdef __cplusplus
}
#endif
