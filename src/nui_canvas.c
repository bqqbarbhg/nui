#include "nui_canvas.h"

typedef struct nui_font_batch nui_font_batch;

struct nui_canvas {
	nui_renderer *renderer;

	nui_layer **layers;
	uint32_t num_layers, cap_layers;

	nui_font **fonts;
	uint32_t num_fonts, cap_fonts;
};

struct nui_font {
	nui_canvas *canvas;
	nui_renderer *renderer;
	uint32_t index;
	uint32_t refcount;
	nui_font_desc desc;
};

typedef struct nui_child {
	nui_layer *layer;
	nui_point offset;
	uint32_t draw_pos;
} nui_child;

struct nui_layer {
	nui_canvas *canvas;
	uint32_t index;
	nui_color bg_color;

	nui_extent size;

	char *draws;
	uint32_t draws_pos, draws_cap;
	int32_t render_pos;

	nui_child *children;
	uint32_t num_children, cap_children;

	nui_layer *parent;

	nui_invalidation inv;
};

static void nui_invalidate(nui_layer *l, nui_invalidation inv) {
	if (inv > l->inv) l->inv = inv;
	nui_layer *parent = l->parent;
	for (; parent != NULL; parent = parent->parent) {
		if (parent->inv >= nui_inv_child) break;
		parent->inv = nui_inv_child;
	}
}

// nui_canvas

nui_canvas *nui_make_canvas(nui_renderer *renderer)
{
	nui_canvas *c = nui_make(nui_canvas);
	c->renderer = renderer;

	return c;
}

void nui_free_canvas(nui_canvas *c)
{
	if (c == NULL) return;

	c->renderer->free(c->renderer);

	for (uint32_t i = 0; i < c->num_layers; i++) {
		if (c->layers[i] != NULL) {
			nui_free_layer(c->layers[i]);
		}
	}
	nui_free(c);
}

// nui_layer

nui_layer *nui_make_layer(nui_canvas *c, nui_extent size)
{
	nui_layer *l = nui_make(nui_layer);
	l->canvas = c;

	l->render_pos = -1;

	l->size.x = size.x;
	l->size.y = size.y;

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
	if (l == NULL) return;

	nui_canvas *c = l->canvas;
	c->layers[l->index] = NULL;
	nui_free(l->draws);
	nui_free(l);
}

nui_extent nui_layer_size(const nui_layer *l)
{
	return l->size;
}

void nui_resize_layer(nui_layer *l, nui_extent size)
{
	if (size.x == l->size.x && size.y == l->size.y) return;
	l->size = size;

	nui_invalidate(l, nui_inv_self);
	if (l->parent) {
		nui_invalidate(l->parent, nui_inv_resize);
	}
}

void nui_set_bg_color(nui_layer *l, nui_color color)
{
	if (nui_color_eq(l->bg_color, color)) return;
	l->bg_color = color;

	nui_invalidate(l, nui_inv_self);
}

uint32_t nui_layer_index(const nui_layer *l)
{
	return l->index;
}

nui_color nui_layer_bg_color(const nui_layer *l)
{
	return l->bg_color;
}

nui_canvas *nui_layer_canvas(const nui_layer *l)
{
	return l->canvas;
}

nui_renderer *nui_layer_renderer(const nui_layer *l)
{
	return l->canvas->renderer;
}

// nui_font

nui_font *nui_make_font(nui_canvas *c, const nui_font_desc *desc)
{
	// Try to find an existing equivalent font
	uint32_t index = c->num_fonts;
	for (uint32_t i = 0; i < c->num_fonts; i++) {
		nui_font *font = c->fonts[i];
		if (font == NULL) {
			if (index == c->num_fonts) {
				index = i;
			}
			continue;
		}

		if (strcmp(font->desc.family, desc->family) != 0) continue;
		if (font->desc.height != desc->height) continue;

		font->refcount++;
		return font;
	}
	if (index == c->num_fonts) {
		nui_buf_grow(&c->fonts, &c->cap_fonts, ++c->num_fonts);
	}

	// Combined allocation with name copy and font struct
	size_t family_len = strlen(desc->family);
	size_t size = sizeof(nui_font) + (family_len + 1);

	nui_font *font = nui_alloc(size);
	font->desc = *desc;
	font->index = index;
	font->canvas = c;
	font->renderer = c->renderer;
	font->refcount = 1;

	char *data = (char*)font + sizeof(nui_font);

	// Overwrite family with copy
	memcpy(data, desc->family, family_len + 1);
	font->desc.family = data;
	data += family_len + 1;

	c->renderer->make_font(c->renderer, index, desc);

	return font;
}

void nui_free_font(nui_font *font)
{
	if (font == NULL) return;
	if (--font->refcount > 0) return;

	nui_canvas *c = font->canvas;
	c->fonts[font->index] = NULL;
	nui_free(font);
}

nui_extent nui_measure_len(nui_font *font, const char *text, size_t len)
{
	return font->renderer->measure(font->renderer, font->index, text, len);
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
	l->draws_pos = 0;

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
	if (l->render_pos - (int32_t)pos >= (int32_t)size) {
		nui_rect_draw *draw = (nui_rect_draw*)(l->draws + pos);
		if (draw->draw.type == nui_dt_rect
			&& nui_rect_eq(&draw->draw.bounds, r)
			&& nui_color_eq(draw->color, color)) {
			return;
		}
	}

	// Invalidate last draws and insert
	l->render_pos = -1;
	nui_buf_grow_uninit(&l->draws, &l->draws_cap, l->draws_pos);
	nui_rect_draw *draw = (nui_rect_draw*)(l->draws + pos);
	draw->draw.type = nui_dt_rect;
	draw->draw.size = size;
	draw->draw.bounds = *r;
	draw->color = color;
}

void nui_draw_text_len(nui_layer *l, nui_point p, nui_font *font, nui_color color, const char *text, size_t len)
{
	uint32_t size = align_draw_size(sizeof(nui_text_draw) + len);
	uint32_t pos = l->draws_pos;
	l->draws_pos = pos + size;
	uint32_t font_index = font->index;

	// Try to re-use old draw
	if (l->render_pos - (int32_t)pos >= (int32_t)size) {
		nui_text_draw *draw = (nui_text_draw*)(l->draws + pos);
		if (draw->draw.type == nui_dt_text
			&& nui_point_eq(draw->draw.bounds.min, p)
			&& draw->font == font_index
			&& nui_color_eq(draw->color, color)
			&& draw->text_len == len
			&& !memcmp(draw->text, text, len)) {
			return;
		}
	}

	nui_extent extent = nui_measure_len(font, text, len);

	// Invalidate last draws and insert
	l->render_pos = -1;
	nui_buf_grow_uninit(&l->draws, &l->draws_cap, l->draws_pos);
	nui_text_draw *draw = (nui_text_draw*)(l->draws + pos);
	draw->draw.type = nui_dt_text;
	draw->draw.size = size;
	draw->draw.bounds.min = p;
	draw->draw.bounds.max.x = p.x + extent.x;
	draw->draw.bounds.max.y = p.y + extent.y;
	draw->font = font_index;
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
	if (l->render_pos - (int32_t)pos >= (int32_t)size) {
		nui_layer_draw *draw = (nui_layer_draw*)(l->draws + pos);
		if (draw->draw.type == nui_dt_layer
			&& nui_point_eq(draw->draw.bounds.min, p)
			&& draw->layer == inner) {

			// This must mean the child is in sync too!
			nui_assert(l->num_children < l->cap_children);
			nui_assert(l->children[child_ix].layer == inner);
			nui_assert(nui_point_eq(l->children[child_ix].offset, p));
			nui_assert(l->children[child_ix].draw_pos == pos);

			return;
		}
	}

	// Invalidate last draws and insert
	l->render_pos = -1;
	nui_buf_grow_uninit(&l->draws, &l->draws_cap, l->draws_pos);
	nui_layer_draw *draw = (nui_layer_draw*)(l->draws + pos);
	draw->draw.type = nui_dt_layer;
	draw->draw.size = size;
	draw->draw.bounds.min = p;
	draw->draw.bounds.max.x = p.x + inner->size.x;
	draw->draw.bounds.max.y = p.y + inner->size.y;
	draw->layer = inner;

	// Add child to layer
	nui_buf_grow_uninit(&l->children, &l->cap_children, l->num_children);
	nui_child *child = &l->children[child_ix];
	child->layer = inner;
	child->offset = p;
	child->draw_pos = pos;
}

// Rendering

void nui_begin_rendering(nui_canvas *c)
{
	// Gather dirty layers
	for (uint32_t i = 0; i < c->num_layers; i++) {
		nui_layer *l = c->layers[i];
		if (l == NULL) continue;
		int32_t pos = (int32_t)l->draws_pos;
		if (pos != l->render_pos) {
			nui_invalidate(l, nui_inv_self);
			l->render_pos = pos;
		}
	}

	// Update child draw bounds
	for (uint32_t li = 0; li < c->num_layers; li++) {
		nui_layer *l = c->layers[li];
		if (l == NULL && l->inv < nui_inv_resize) continue;

		uint32_t num_children = l->num_children;
		for (uint32_t i = 0; i < num_children; i++) {
			nui_child *child = &l->children[i];
			nui_layer *cl = child->layer;
			nui_draw *draw = (nui_draw*)(l->draws + child->draw_pos);
			draw->bounds.right = draw->bounds.left + cl->size.x;
			draw->bounds.bottom = draw->bounds.top + cl->size.y;
		}
	}
}

void nui_end_rendering(nui_canvas *c)
{
	for (uint32_t i = 0; i < c->num_layers; i++) {
		nui_layer *l = c->layers[i];
		if (l == NULL) continue;
		l->inv = nui_inv_none;
	}
}

nui_invalidation nui_layer_invalidation(nui_layer *l)
{
	return l->inv;
}

nui_draw *nui_draws_begin(nui_layer *l)
{
	return (nui_draw*)l->draws;
}

nui_draw *nui_draws_end(nui_layer *l)
{
	return (nui_draw*)(l->draws + l->draws_pos);
}
