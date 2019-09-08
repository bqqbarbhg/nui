#pragma once

#include <stdint.h>
#include <string.h>

typedef struct nui_context_s nui_context;
typedef struct nui_layer_s nui_layer;
typedef struct nui_container_s nui_container;
typedef struct nui_font_s nui_font;
typedef struct nui_area_s nui_area;
typedef struct nui_window_s nui_window;

typedef struct {
	uint8_t r, g, b, a;
} nui_color;

typedef struct {
	int32_t x, y;
} nui_point;

typedef struct {
	int32_t x, y;
} nui_extent;

typedef struct {
	nui_point min, max;
} nui_bounds;

typedef struct {
	nui_layer *layer;
	union {
		struct {
			nui_point min, max;
		};
		nui_bounds bounds;
	};
} nui_rect;

typedef struct {
	nui_layer *layer;
} nui_event;

typedef struct {
	nui_event event;
	nui_point pos;
} nui_mouse_event;

typedef struct {
	nui_event event;
	int32_t delta;
} nui_scroll_event;

typedef enum {
	nui_drag_begin,
	nui_drag_dragging,
	nui_drag_end,
} nui_drag_state;

typedef struct {
	nui_event event;
	nui_point begin, end;
	nui_extent delta;
	nui_drag_state state;
} nui_drag_event;

typedef enum {
	nui_left,
	nui_top,
	nui_right,
	nui_bottom,
} nui_dir;

typedef enum {
	nui_cursor_default,
	nui_cursor_click,
	nui_cursor_resize_n,
	nui_cursor_resize_e,
	nui_cursor_resize_s,
	nui_cursor_resize_w,
	nui_cursor_resize_ne,
	nui_cursor_resize_nw,
	nui_cursor_resize_se,
	nui_cursor_resize_sw,
	nui_cursor_resize_n_s,
	nui_cursor_resize_e_w,
	nui_cursor_resize_ne_sw,
	nui_cursor_resize_nw_se,
} nui_cursor;

typedef enum {
	nui_flex_none,
	nui_flex_horizontal,
	nui_flex_vertical,
	nui_flex_both,
} nui_flex;

typedef void (*nui_render_fn)(void *user);
typedef int (*nui_event_handler_fn)(void *user, nui_event *event);
typedef void (*nui_container_update_fn)(void *user, nui_container *container, nui_extent old_size, nui_extent new_size);

typedef struct {
	const char *family;
	int32_t height;
} nui_font_desc;

typedef struct {
	int32_t width, height;
} nui_window_desc;

#define NUI_TIMEOUT_INSTANT ((uint64_t)0)
#define NUI_TIMEOUT_INFINITE (~(uint64_t)0)

// Misc helpers

static nui_color nui_rgb(uint32_t hex) {
	nui_color c = { hex >> 16 & 0xff, hex >> 8 & 0xff, hex & 0xff, 0xff };
	return c;
}

static nui_color nui_white() {
	nui_color c = { 0xff, 0xff, 0xff, 0xff };
	return c;
}

static nui_color nui_black() {
	nui_color c = { 0x00, 0x00, 0x00, 0x00 };
	return c;
}

static nui_dir nui_opposite_dir(nui_dir dir) {
	return (nui_dir)(((uint32_t)dir + 2) & 3);
}

static nui_point nui_pt(int32_t x, int32_t y) {
	nui_point p = { x, y };
	return p;
}

static nui_extent nui_ex(int32_t x, int32_t y) {
	nui_extent p = { x, y };
	return p;
}

static int32_t nui_min(int32_t a, int32_t b) {
	return a < b ? a : b;
}

static int32_t nui_max(int32_t a, int32_t b) {
	return a > b ? a : b;
}

static int32_t nui_clamp(int32_t a, int32_t min_edge, int32_t max_edge) {
	return a < min_edge ? min_edge : (a > max_edge ? max_edge : a);
}

int nui_intersects(nui_bounds a, nui_bounds b);
int nui_contains(nui_bounds b, nui_point p);

void nui_printf(nui_context *ctx, const char *fmt, ...);

// Rects

int nui_rect_eq(nui_rect a, nui_rect b);

nui_rect nui_split(nui_rect *rest, nui_rect parent, int32_t offset, nui_dir dir);
nui_rect nui_edge(nui_rect parent, int32_t offset, nui_dir dir);
nui_rect nui_push(nui_rect *total_area, int32_t offset, nui_dir dir);
nui_rect nui_relative(nui_rect parent, double left, double top, double right, double bottom);

static int32_t nui_width(nui_rect rect) { return rect.max.x - rect.min.x; }
static int32_t nui_height(nui_rect rect) { return rect.max.y - rect.min.y; }
static nui_extent nui_size(nui_rect rect) {
	nui_extent p = { rect.max.x - rect.min.x, rect.max.y - rect.min.y };
	return p;
}
int32_t nui_distance_to_edge(nui_rect rect, nui_point point, nui_dir dir);
int32_t nui_extent_to_dir(nui_rect rect, nui_dir dir);

// Fonts

nui_font *nui_make_font(nui_context *ctx, const nui_font_desc *desc);
void nui_free_font(nui_font *font);
nui_extent nui_measure_text_len(nui_font *font, const char *str, size_t len);
static nui_extent nui_measure_text(nui_font *font, const char *str) {
	return nui_measure_text_len(font, str, strlen(str));
}

// Layers

nui_layer *nui_make_layer(nui_context *ctx, nui_render_fn fn, void *user, nui_flex flex);
void nui_free_layer(nui_layer *layer);
void nui_resize_layer(nui_layer *layer, int32_t width, int32_t height);
void nui_resize_layer_x(nui_layer *layer, int32_t width);
void nui_resize_layer_y(nui_layer *layer, int32_t height);
void nui_layer_set_debug_name(nui_layer *layer, const char *name);

nui_rect nui_layer_rect(nui_layer *layer);

void nui_clear(nui_layer *layer);
void nui_invalidate(nui_layer *layer);

// Containers

nui_container *nui_make_container();
void nui_free_container(nui_container *container);
void nui_resize_container(nui_container *container, int32_t width, int32_t height);
nui_extent nui_container_size(nui_container *container);
void nui_bind_layer(nui_container *container, nui_layer *layer);

void nui_container_on_update(nui_container *container, nui_container_update_fn fn, void *user);

// Areas

nui_area *nui_make_area(nui_context *ctx);
void nui_free_area(nui_area *area);
void nui_area_scroll(nui_area *area, nui_event_handler_fn fn, void *user);
void nui_area_drag(nui_area *area, nui_event_handler_fn fn, void *user);

void nui_add_area(nui_rect rect, nui_area *area);

// Drawing

void nui_fill_rect(nui_rect rect, nui_color color);
void nui_draw_text_len(nui_rect rect, nui_font *font, nui_color color, const char *text, size_t len);
static void nui_draw_text(nui_rect rect, nui_font *font, nui_color color, const char *text) {
	nui_draw_text_len(rect, font, color, text, strlen(text));
}
void nui_draw_layer(nui_rect rect, nui_layer *layer);
void nui_draw_layer_offset(nui_rect rect, nui_layer *layer, nui_extent offset);
void nui_draw_container(nui_rect rect, nui_container *container);
void nui_draw_container_offset(nui_rect rect, nui_container *container, nui_extent offset);

// Windows

nui_window *nui_make_window(nui_context *ctx, const nui_window_desc *desc);
void nui_free_window(nui_window *window);
nui_container *nui_window_container(nui_window *window);
int nui_is_open(nui_window *window);

// Event loop

int nui_event_loop(nui_context *ctx, uint64_t timeout_ns);
void nui_render_layers(nui_context *ctx);
