#include "nui.h"
#include "nui_backend.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

#ifndef NUI_MIN_ALLOC
#define NUI_MIN_ALLOC 128
#endif

// Definitions

typedef struct {
	nui_area *area;
	nui_bounds bounds;
} nui_rect_area;

struct nui_container_s {
	nui_layer *bound_layer;

	nui_extent size;

	nui_container_update_fn update_fn;
	void *update_user;
};

struct nui_layer_s {
	char *debug_name;

	nui_context *ctx;

	nui_container *bound_container;

	nui_render_fn render_fn;
	void *render_user;

	nui_flex flex;
	nui_extent size;

	char *draw_buf;
	uint32_t draw_pos;
	uint32_t draw_cap;

	nui_rect_area *areas;
	uint32_t num_areas;
	uint32_t cap_areas;

	int cull_enabled;
	nui_bounds cull_bounds;

	nui_layer *render_prev, *render_next;
	int invalidated;
};

struct nui_area_s {
	nui_context *ctx;

	nui_event_handler_fn scroll_handler;
	void *scroll_user;
	nui_event_handler_fn drag_handler;
	void *drag_user;
};

struct nui_window_s {
	nui_context *ctx;
	void *be_window;

	nui_container container;

	int is_open;
};

struct nui_context_s {
	nui_backend be;
	void *user;

	nui_layer *render_first, *render_last;
};

// Utilities

int nui_intersects(nui_bounds a, nui_bounds b)
{
	return !(a.max.x <= b.min.x || a.max.y <= b.min.y\
		|| a.min.x >= b.max.x || a.min.y >= b.max.y);
}

int nui_contains(nui_bounds b, nui_point p)
{
	return p.x >= b.min.x && p.y >= b.min.y
		&& p.x < b.max.x && p.y < b.max.x;
}

void nui_printf(nui_context *ctx, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char line[256];
	int len = vsnprintf(line, sizeof(line), fmt, args);

	va_end(args);

	ctx->be.debug_print(ctx->user, line, (size_t)len);
}

void *nui_alloc(size_t size)
{
	void *ptr = malloc(size);
	memset(ptr, 0, size);
	return ptr;
}

void *nui_alloc_uninit(size_t size)
{
	return malloc(size);
}

void *nui_realloc_uninit(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

void nui_free(void *ptr)
{
	free(ptr);
}

void nui_buf_realloc(void **data, uint32_t *p_cap, size_t size)
{
	uint32_t old_cap = *p_cap, cap = old_cap;
	cap = cap ? cap * 2 : NUI_MIN_ALLOC / (uint32_t)size;
	if (cap == 0) cap = 1;
	*p_cap = cap;
	*data = nui_realloc_uninit(*data, cap * size);
	memset((char*)*data + old_cap * size, 0, (cap - old_cap) * size);
}

void nui_buf_realloc_uninit(void **data, uint32_t *p_cap, size_t size)
{
	uint32_t cap = *p_cap;
	cap = cap ? cap * 2 : NUI_MIN_ALLOC / (uint32_t)size;
	if (cap == 0) cap = 1;
	*p_cap = cap;
	*data = nui_realloc_uninit(*data, cap * size);
}

// Rects

int nui_rect_eq(nui_rect a, nui_rect b)
{
	return !memcmp(&a, &b, sizeof(nui_rect));
}

nui_rect nui_split(nui_rect *rest, nui_rect parent, int32_t offset, nui_dir dir)
{
	nui_rect r;
	r.layer = parent.layer;
	rest->layer = parent.layer;
	switch (dir) {
	case nui_left:
		r.min.y = rest->min.y = parent.min.y;
		r.max.y = rest->max.y = parent.max.y;
		r.min.x = parent.min.x;
		rest->min.x = r.max.x = nui_clamp(parent.min.x + offset, parent.min.x, parent.max.x);
		rest->max.x = parent.max.x;
		break;
	case nui_top:
		r.min.x = rest->min.x = parent.min.x;
		r.max.x = rest->max.x = parent.max.x;
		r.min.y = parent.min.y;
		rest->min.y = r.max.y = nui_clamp(parent.min.y + offset, parent.min.y, parent.max.y);
		rest->max.y = parent.max.y;
		break;
	case nui_right:
		r.min.y = rest->min.y = parent.min.y;
		r.max.y = rest->max.y = parent.max.y;
		r.max.x = parent.max.x;
		rest->max.x = r.min.x = nui_clamp(parent.max.x - offset, parent.min.x, parent.max.x);
		rest->min.x = parent.min.x;
		break;
	case nui_bottom:
		r.min.x = rest->min.x = parent.min.x;
		r.max.x = rest->max.x = parent.max.x;
		r.max.y = parent.max.y;
		rest->max.y = r.min.y = nui_clamp(parent.max.y - offset, parent.min.y, parent.max.y);
		rest->min.y = parent.min.y;
		break;
	default:
		assert(0 && "Invalid dir");
		break;
	}
	return r;
}

nui_rect nui_edge(nui_rect parent, int32_t offset, nui_dir dir)
{
	nui_rect r;
	r.layer = parent.layer;
	switch (dir) {
	case nui_left:
		r.min.y = parent.min.y;
		r.max.y = parent.max.y;
		r.min.x = parent.min.x;
		r.max.x = nui_clamp(parent.min.x + offset, parent.min.x, parent.max.x);
		break;
	case nui_top:
		r.min.x = parent.min.x;
		r.max.x = parent.max.x;
		r.min.y = parent.min.y;
		r.max.y = nui_clamp(parent.min.y + offset, parent.min.y, parent.max.y);
		break;
	case nui_right:
		r.min.y = parent.min.y;
		r.max.y = parent.max.y;
		r.max.x = parent.max.x;
		r.min.x = nui_clamp(parent.max.x - offset, parent.min.x, parent.max.x);
		break;
	case nui_bottom:
		r.min.x = parent.min.x;
		r.max.x = parent.max.x;
		r.max.y = parent.max.y;
		r.min.y = nui_clamp(parent.max.y - offset, parent.min.y, parent.max.y);
		break;
	default:
		assert(0 && "Invalid dir");
		break;
	}
	return r;
}

nui_rect nui_push(nui_rect *total_area, int32_t offset, nui_dir dir)
{
	nui_rect r;
	r.layer = total_area->layer;
	switch (dir) {
	case nui_left:
		r.min.y = total_area->min.y;
		r.max.y = total_area->max.y;
		r.max.x = total_area->min.x;
		total_area->min.x = r.min.x = total_area->min.x - nui_max(offset, 0);
		break;
	case nui_top:
		r.min.x = total_area->min.x;
		r.max.x = total_area->max.x;
		r.max.y = total_area->min.y;
		total_area->min.y = r.min.y = total_area->min.y - nui_max(offset, 0);
		break;
	case nui_right:
		r.min.y = total_area->min.y;
		r.max.y = total_area->max.y;
		r.min.x = total_area->max.x;
		total_area->max.x = r.max.x = total_area->max.x + nui_max(offset, 0);
		break;
	case nui_bottom:
		r.min.x = total_area->min.x;
		r.max.x = total_area->max.x;
		r.min.y = total_area->max.y;
		total_area->max.y = r.max.y = total_area->max.y + nui_max(offset, 0);
		break;
	default:
		assert(0 && "Invalid dir");
		break;
	}
	return r;
}

nui_rect nui_relative(nui_rect parent, double left, double top, double right, double bottom)
{
	nui_rect r;
	r.layer = parent.layer;

	double w = (double)(parent.max.x - parent.min.x);
	double h = (double)(parent.max.y - parent.min.y);
	r.min.x = nui_clamp(parent.min.x + (int32_t)(left * w), parent.min.x, parent.max.x);
	r.min.y = nui_clamp(parent.min.y + (int32_t)(top * h), parent.min.y, parent.max.y);
	r.max.x = nui_clamp(parent.min.x + (int32_t)(right * w), parent.min.x, parent.max.x);
	r.max.y = nui_clamp(parent.min.y + (int32_t)(bottom * h), parent.min.y, parent.max.y);
	return r;
}

int32_t nui_distance_to_edge(nui_rect rect, nui_point point, nui_dir dir)
{
	switch (dir) {
	case nui_left:
		return nui_max(point.x - rect.min.x, 0);
	case nui_top:
		return nui_max(point.y - rect.min.y, 0);
	case nui_right:
		return nui_max(rect.max.x - point.x, 0);
	case nui_bottom:
		return nui_max(rect.max.y - point.y, 0);
	default:
		assert(0 && "Invalid dir");
		return 0;
	}
}

int32_t nui_extent_to_dir(nui_rect rect, nui_dir dir)
{
	switch (dir) {
	case nui_left:
	case nui_right:
		return rect.max.x - rect.min.x;
	case nui_top:
	case nui_bottom:
		return rect.max.y - rect.min.y;
	default:
		assert(0 && "Invalid dir");
		return 0;
	}
}

// Internal

static nui_draw *nui_push_draw(nui_rect rect, nui_draw_type type, size_t size) {
	nui_layer *layer = rect.layer;
	if (layer->cull_enabled && nui_intersects(layer->cull_bounds, rect.bounds)) {
		return NULL;
	}

	size = (size + 7u) & ~7u;
	assert(size <= UINT32_MAX);
	uint32_t pos = layer->draw_pos;
	uint32_t next_pos = pos + (uint32_t)size;
	layer->draw_pos = next_pos;
	nui_buf_grow_uninit(&layer->draw_buf, &layer->draw_cap, next_pos);
	nui_draw *draw = (nui_draw*)(layer->draw_buf + pos);
	draw->type = type;
	draw->size = (uint32_t)size;
	draw->bounds = rect.bounds;
	return draw;
}

// Fonts

nui_font *nui_make_font(nui_context *ctx, const nui_font_desc *desc)
{
	// TODO
	return NULL;
}

void nui_free_font(nui_font *font)
{
	nui_free(font);
}

nui_extent nui_measure_text_len(nui_font *font, const char *str, size_t len)
{
	return nui_ex(0, 0);
}

// Layers

nui_layer *nui_make_layer(nui_context *ctx, nui_render_fn fn, void *user, nui_flex flex)
{
	nui_layer *layer = (nui_layer*)nui_alloc(sizeof(nui_layer));
	layer->ctx = ctx;
	layer->render_fn = fn;
	layer->render_user = user;
	layer->flex = flex;
	return layer;
}

void nui_free_layer(nui_layer *layer)
{
	nui_free(layer->debug_name);
	nui_free(layer->draw_buf);
	nui_free(layer);
}

void nui_resize_layer(nui_layer *layer, int32_t width, int32_t height)
{
	if (width != layer->size.x || height != layer->size.y) {
		nui_extent old_size = layer->size;
		layer->size.x = width;
		layer->size.y = height;
		nui_invalidate(layer);
		nui_container *container = layer->bound_container;
		if (container && container->update_fn) {
			container->update_fn(container->update_user, container, old_size, layer->size);
		}
	}
}

void nui_resize_layer_x(nui_layer *layer, int32_t width)
{
	if (width != layer->size.x) {
		nui_extent old_size = layer->size;
		layer->size.x = width;
		nui_invalidate(layer);
		nui_container *container = layer->bound_container;
		if (container && container->update_fn) {
			container->update_fn(container->update_user, container, old_size, layer->size);
		}
	}
}

void nui_resize_layer_y(nui_layer *layer, int32_t height)
{
	if (height != layer->size.y) {
		nui_extent old_size = layer->size;
		layer->size.y = height;
		nui_invalidate(layer);
		nui_container *container = layer->bound_container;
		if (container && container->update_fn) {
			container->update_fn(container->update_user, container, old_size, layer->size);
		}
	}
}

void nui_layer_set_debug_name(nui_layer *layer, const char *name)
{
	size_t len = strlen(name) + 1;
	char *copy = nui_alloc(len);
	memcpy(copy, name, len);
	layer->debug_name = copy;
}

nui_rect nui_layer_rect(nui_layer *layer)
{
	nui_rect r = { layer };
	r.min.x = 0;
	r.min.y = 0;
	r.max.x = layer->size.x;
	r.max.y = layer->size.y;
	return r;
}

void nui_clear(nui_layer *layer)
{
	layer->draw_pos = 0;
	layer->num_areas = 0;
}

void nui_invalidate(nui_layer *layer)
{
	nui_context *ctx = layer->ctx;
	if (layer->invalidated) return;
	layer->invalidated = 1;
	layer->render_next = NULL;
	if (ctx->render_last) {
		ctx->render_last->render_next = layer;
		layer->render_prev = ctx->render_last;
		ctx->render_last = layer;
	} else {
		ctx->render_first = layer;
		ctx->render_last = layer;
	}
}

// Containers

nui_container *nui_make_container()
{
	nui_container *container = (nui_container*)nui_alloc(sizeof(nui_container));
	return container;
}

void nui_free_container(nui_container *container)
{
	nui_free(container);
}

void nui_resize_container(nui_container *container, int32_t width, int32_t height)
{
	if (width != container->size.x || height != container->size.y) {
		container->size.x = width;
		container->size.y = height;
		if (container->bound_layer) {
			nui_resize_layer(container->bound_layer, width, height);
			nui_invalidate(container->bound_layer);
		}
	}
}

nui_extent nui_container_size(nui_container *container)
{
	if (container->bound_layer) {
		return container->bound_layer->size;
	} else {
		return nui_ex(0, 0);
	}
}

void nui_bind_layer(nui_container *container, nui_layer *layer)
{
	container->bound_layer = layer;
	layer->bound_container = container;
	nui_resize_layer(layer, container->size.x, container->size.y);
	nui_invalidate(layer);
}

void nui_container_on_update(nui_container *container, nui_container_update_fn fn, void *user)
{
	container->update_fn = fn;
	container->update_user = user;
}

// Areas

nui_area *nui_make_area(nui_context *ctx)
{
	nui_area *area = (nui_area*)nui_alloc(sizeof(nui_area));
	area->ctx = ctx;
	return area;
}

void nui_free_area(nui_area *area)
{
	nui_free(area);
}

void nui_area_scroll(nui_area *area, nui_event_handler_fn fn, void *user)
{
	area->scroll_handler = fn;
	area->scroll_user = fn;
}

void nui_area_drag(nui_area *area, nui_event_handler_fn fn, void *user)
{
	area->drag_handler = fn;
	area->drag_user = fn;
}

void nui_add_area(nui_rect rect, nui_area *area)
{
	nui_layer *layer = rect.layer;
	uint32_t index = layer->num_areas++;
	nui_buf_grow_uninit(&layer->areas, &layer->cap_areas, layer->num_areas);
	nui_rect_area *ra = &layer->areas[index];
	ra->area = area;
	ra->bounds = rect.bounds;
}

// Drawing

void nui_fill_rect(nui_rect rect, nui_color color)
{
	nui_rect_draw *draw = (nui_rect_draw*)nui_push_draw(rect, nui_dt_rect, sizeof(nui_rect_draw));
	if (!draw) return;
	draw->color = color;
}

void nui_draw_text_len(nui_rect rect, nui_font *font, nui_color color, const char *text, size_t len)
{
	assert(len <= UINT32_MAX);
	nui_text_draw *draw = (nui_text_draw*)nui_push_draw(rect, nui_dt_text, sizeof(nui_text_draw) + len);
	if (!draw) return;
	draw->font = font;
	draw->color = color;
	draw->text_len = (uint32_t)len;
	memcpy(draw->text, text, len + 1);
}

void nui_draw_layer(nui_rect rect, nui_layer *layer)
{
	nui_draw_layer_offset(rect, layer, nui_ex(0, 0));
}

void nui_draw_layer_offset(nui_rect rect, nui_layer *layer, nui_extent offset)
{
	nui_layer_draw *draw = (nui_layer_draw*)nui_push_draw(rect, nui_dt_layer, sizeof(nui_layer_draw));
	if (!draw) return;
	draw->layer = layer;
	draw->offset = offset;

	switch (layer->flex) {
	case nui_flex_none:
		nui_resize_layer(layer, nui_width(rect), nui_height(rect));
		break;
	case nui_flex_vertical:
		nui_resize_layer_x(layer, nui_width(rect));
		break;
	case nui_flex_horizontal:
		nui_resize_layer_y(layer, nui_height(rect));
		break;
	}
}

void nui_draw_container(nui_rect rect, nui_container *container)
{
	if (!container->bound_layer) return;
	nui_draw_layer_offset(rect, container->bound_layer, nui_ex(0, 0));
}

void nui_draw_container_offset(nui_rect rect, nui_container *container, nui_extent offset)
{
	if (!container->bound_layer) return;
	nui_draw_layer_offset(rect, container->bound_layer, offset);
}

// Windows

nui_window *nui_make_window(nui_context *ctx, const nui_window_desc *desc)
{
	nui_window *window = (nui_window*)nui_alloc(sizeof(nui_window));
	window->ctx = ctx;

	window->be_window = ctx->be.make_window(ctx->user, window, desc);
	window->is_open = 1;

	return window;
}

void nui_free_window(nui_window *window)
{
	nui_free(window);
}

nui_container *nui_window_container(nui_window *window)
{
	return &window->container;
}

int nui_is_open(nui_window *window)
{
	return window->is_open;
}

// Event loop

int nui_event_loop(nui_context *ctx, uint64_t timeout_ns)
{
	return ctx->be.event_loop(ctx->user, timeout_ns);
}

void nui_render_layers(nui_context *ctx)
{
	for (size_t iter = 0; iter < 100; iter++) {
		nui_layer *layer = ctx->render_first;
		if (!layer) break;

		ctx->render_first = layer->render_next;
		if (ctx->render_first) {
			ctx->render_first->render_prev = NULL;
		} else {
			ctx->render_last = NULL;
		}
		layer->invalidated = 0;

		if (layer->size.x > 0 || layer->size.y > 0) {
			nui_printf(ctx, "Rendering %s %dx%d\n", layer->debug_name, layer->size.x, layer->size.y);

			nui_clear(layer);
			layer->render_fn(layer->render_user);
		}
	}
}

// Backends

nui_draw *nui_be_draws_begin(nui_layer *layer)
{
	return (nui_draw*)layer->draw_buf;
}

nui_draw *nui_be_draws_end(nui_layer *layer)
{
	return (nui_draw*)(layer->draw_buf + layer->draw_pos);
}

nui_layer *nui_be_window_layer(nui_window *window)
{
	return window->container.bound_layer;
}

nui_context *nui_be_make_context(const nui_backend *be, void *user)
{
	nui_context *ctx = (nui_context*)nui_alloc(sizeof(nui_context));
	ctx->be	= *be;
	ctx->user = user;
	return ctx;
}
