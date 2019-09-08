#pragma once

#include "nui.h"

void *nui_alloc(size_t size);
void *nui_alloc_uninit(size_t size);
void *nui_realloc_uninit(void *ptr, size_t size);
void nui_free(void *ptr);

void nui_buf_realloc(void **p_data, uint32_t *p_cap, size_t size);
void nui_buf_realloc_uninit(void **p_data, uint32_t *p_cap, size_t size);

static void nui_buf_grow_size(void **p_data, uint32_t *p_cap, uint32_t num, size_t size) {
	if (num <= *p_cap) return;
	nui_buf_realloc(p_data, p_cap, size);
}

static void nui_buf_grow_size_uninit(void **p_data, uint32_t *p_cap, uint32_t num, size_t size) {
	if (num <= *p_cap) return;
	nui_buf_realloc_uninit(p_data, p_cap, size);
}

#define nui_buf_grow(p_buf, p_cap, num) nui_buf_grow_size((void**)(p_buf), (p_cap), (num), sizeof(**(p_buf)))
#define nui_buf_grow_uninit(p_buf, p_cap, num) nui_buf_grow_size_uninit((void**)(p_buf), (p_cap), (num), sizeof(**(p_buf)))

typedef enum {
	nui_dt_rect,
	nui_dt_text,
	nui_dt_layer,
} nui_draw_type;

typedef struct {
	nui_draw_type type;
	uint32_t size;
	nui_bounds bounds;
} nui_draw;

typedef struct {
	nui_draw draw;
	nui_color color;
} nui_rect_draw;

typedef struct {
	nui_draw draw;
	nui_color color;
	nui_font *font;
	uint32_t text_len;
	char text[1];
} nui_text_draw;

typedef struct {
	nui_draw draw;
	nui_layer *layer;
	nui_extent offset;
} nui_layer_draw;

typedef struct {
	void *(*make_window)(void *user, nui_window *window, const nui_window_desc *desc);
	int (*event_loop)(void *user, uint64_t timeout_ns);

	void (*debug_print)(void *user, const char *str, size_t length);
} nui_backend;

nui_draw *nui_be_draws_begin(nui_layer *layer);
nui_draw *nui_be_draws_end(nui_layer *layer);

static nui_draw *nui_be_next_draw(nui_draw *draw) {
	return (nui_draw*)((char*)draw + draw->size);
}

nui_layer *nui_be_window_layer(nui_window *window);

nui_context *nui_be_make_context(const nui_backend *be, void *user);
