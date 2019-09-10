#include "nui_canvas.h"

struct nui_canvas {
	nui_layer **layers;
	uint32_t num_layers, cap_layers;
};

typedef struct nui_child {
	nui_layer *layer;
	nui_point offset;
} nui_child;

struct nui_layer {
	nui_canvas *canvas;
	uint32_t index;

	nui_extent size;

	int32_t render_pos;

	char *draws;
	uint32_t draws_pos, draws_cap;
	int32_t draws_last_pos;

	nui_child *children;
	uint32_t num_children, cap_children;

	nui_layer *parent;

	int dirty;
};

// nui_canvas

nui_canvas *nui_make_canvas()
{
	nui_canvas *c = nui_make(nui_canvas);

	return c;
}

void nui_free_canvas(nui_canvas *c)
{
	for (uint32_t i = 0; i < c->num_layers; i++) {
		if (c->layers[i] != NULL) {
			nui_free_layer(c->layers[i]);
		}
	}
	nui_free(c);
}

// nui_layer

nui_layer *nui_make_layer(nui_canvas *c, int32_t x, int32_t y)
{
	nui_layer *l = nui_make(nui_layer);
	l->canvas = c;

	l->draws_last_pos = -1;
	l->render_pos = -1;

	l->size.x = x;
	l->size.y = y;

	uint32_t index;
	for (index = 0; index < c->num_layers; index++) {
		if (c->layers[index] == NULL) break;
	}
	if (index == c->num_layers) {
		nui_buf_grow(&c->layers, &c->cap_layers, ++c->num_layers);
	}
	l->index = index;
	c->layers[index] = l;

	return l;
}

void nui_free_layer(nui_layer *l)
{
	nui_canvas *c = l->canvas;
	c->layers[l->index] = NULL;
	nui_free(l->draws);
	nui_free(l);
}

nui_extent nui_layer_size(const nui_layer *l)
{
	return l->size;
}

void nui_resize_layer(nui_layer *l, int32_t x, int32_t y)
{
	l->size.x = x;
	l->size.y = y;
}

uint32_t nui_layer_index(const nui_layer *l)
{
	return l->index;
}

// Drawing

static uint32_t align_draw_size(size_t size)
{
	size_t s = (size + 7u) & ~(size_t)7u;
	nui_assert(s <= UINT32_MAX);
	return (uint32_t)s;
}

void nui_clear(nui_layer *l)
{
	if (l->render_pos >= 0) {
		l->draws_last_pos = (int32_t)l->draws_pos;
	} else {
		l->draws_last_pos = -1;
	}
	l->draws_pos = 0;
	l->render_pos = -1;

	uint32_t num_children = l->num_children;
	for (uint32_t i = 0; i < num_children; i++) {
		l->children[i].layer->parent = NULL;
	}

	l->num_children = 0;
}

void nui_fill_rect(nui_layer *l, const nui_rect *r, nui_color color)
{
	uint32_t size = align_draw_size(sizeof(nui_rect_draw));
	uint32_t pos = l->draws_pos;
	l->draws_pos = pos + size;

	// Try to re-use old draw
	if (l->draws_last_pos - (int32_t)pos >= (int32_t)size) {
		nui_rect_draw *draw = (nui_rect_draw*)(l->draws + pos);
		if (draw->draw.type == nui_dt_rect
			&& nui_rect_eq(&draw->rect, r)
			&& nui_color_eq(draw->color, color)) {
			return;
		}
	}

	// Invalidate last draws and insert
	l->draws_last_pos = -1;
	nui_buf_grow_uninit(&l->draws, &l->draws_cap, l->draws_pos);
	nui_rect_draw *draw = (nui_rect_draw*)(l->draws + pos);
	draw->draw.type = nui_dt_rect;
	draw->draw.size = size;
	draw->rect = *r;
	draw->color = color;
}

void nui_draw_text_len(nui_layer *l, nui_point p, nui_color color, const char *text, size_t len)
{
	uint32_t size = align_draw_size(sizeof(nui_text_draw) + len);
	uint32_t pos = l->draws_pos;
	l->draws_pos = pos + size;

	// Try to re-use old draw
	if (l->draws_last_pos - (int32_t)pos >= (int32_t)size) {
		nui_text_draw *draw = (nui_text_draw*)(l->draws + pos);
		if (draw->draw.type == nui_dt_text
			&& nui_point_eq(draw->pos, p)
			&& nui_color_eq(draw->color, color)
			&& draw->text_len == len
			&& !memcmp(draw->text, text, len)) {
			return;
		}
	}

	// Invalidate last draws and insert
	l->draws_last_pos = -1;
	nui_buf_grow_uninit(&l->draws, &l->draws_cap, l->draws_pos);
	nui_text_draw *draw = (nui_text_draw*)(l->draws + pos);
	draw->draw.type = nui_dt_text;
	draw->draw.size = size;
	draw->pos = p;
	draw->color = color;
	draw->text_len = len;
	memcpy(draw->text, text, len);
	draw->text[len] = '\0';
}

void nui_draw_layer(nui_layer *l, nui_point p, nui_layer *inner)
{
	uint32_t size = align_draw_size(sizeof(nui_layer_draw));
	uint32_t pos = l->draws_pos;
	l->draws_pos = pos + size;
	uint32_t child_ix = l->num_children++;

	nui_assert(inner->parent == NULL);
	inner->parent = l;

	// Try to re-use old draw
	if (l->draws_last_pos - (int32_t)pos >= (int32_t)size) {
		nui_layer_draw *draw = (nui_layer_draw*)(l->draws + pos);
		if (draw->draw.type == nui_dt_layer
			&& nui_point_eq(draw->pos, p)
			&& draw->layer == inner) {

			// This must mean the child is in sync too!
			nui_assert(l->num_children < l->cap_children);
			nui_assert(l->children[child_ix].layer == inner);

			return;
		}
	}

	// Invalidate last draws and insert
	l->draws_last_pos = -1;
	nui_buf_grow_uninit(&l->draws, &l->draws_cap, l->draws_pos);
	nui_layer_draw *draw = (nui_layer_draw*)(l->draws + pos);
	draw->draw.type = nui_dt_layer;
	draw->draw.size = size;
	draw->pos = p;
	draw->layer = inner;

	// Add child to layer
	nui_buf_grow_uninit(&l->children, &l->cap_children, l->num_children);
	nui_child *child = &l->children[child_ix];
	child->layer = inner;
	child->offset = p;
}

// Rendering

void nui_begin_rendering(nui_canvas *c)
{
	for (uint32_t i = 0; i < c->num_layers; i++) {
		nui_layer *l = c->layers[i];
		if (l == NULL) continue;
		int32_t pos = (int32_t)l->draws_pos;
		if (pos != l->render_pos) {
			if (pos != l->draws_last_pos) {
				l->dirty = 1;
				nui_layer *parent = l->parent;
				for (; parent != NULL; parent = parent->parent) {
					parent->dirty = 1;
				}
			}
			l->render_pos = pos;
		}
	}
}

void nui_end_rendering(nui_canvas *c)
{
	for (uint32_t i = 0; i < c->num_layers; i++) {
		nui_layer *l = c->layers[i];
		if (l == NULL) continue;
		l->dirty = 0;
	}
}

int nui_layer_dirty(nui_layer *l)
{
	return l->dirty;
}

nui_draw *nui_draws_begin(nui_layer *l)
{
	return (nui_draw*)l->draws;
}

nui_draw *nui_draws_end(nui_layer *l)
{
	return (nui_draw*)(l->draws + l->draws_pos);
}
