#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nui_color {
	union {
		struct {
			uint8_t r, g, b, a;
		};

		// Just for aligning the struct to 4 bytes.
		// Can be used to read all channels _in_ C!
		uint32_t impl_align;
	};
} nui_color;

typedef struct nui_point {
	int32_t x, y;
} nui_point;

typedef struct nui_extent {
	int32_t x, y;
} nui_extent;

typedef enum nui_dir {
	NUI_LEFT,
	NUI_TOP,
	NUI_RIGHT,
	NUI_BOTTOM,

	NUI_DIRS,
} nui_dir;

typedef struct nui_rect {
	union {
		struct {
			nui_point min, max;
		};
		struct {
			int32_t left, top, right, bottom;
		};
		struct {
			int32_t dir[NUI_DIRS];
		};
	};
} nui_rect;

// Generic

#define nui_assert(cond) do { if (!(cond)) __debugbreak(); } while (0)
#define nui_arraysize(arr) (sizeof(arr)/sizeof(*(arr)))

static int32_t nui_min(int32_t x, int32_t y) {
	return x < y ? x : y;
}

static int32_t nui_max(int32_t x, int32_t y) {
	return x > y ? x : y;
}
static int32_t nui_clamp(int32_t x, int32_t min_x, int32_t max_x) {
	return x < min_x ? min_x : x > max_x ? max_x : x;
}

// nui_color

static int nui_color_eq(nui_color a, nui_color b) {
#ifdef __cplusplus
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
#else
	return a.impl_align == b.impl_align;
#endif
}

static nui_color nui_rgb(uint32_t hex) {
	nui_color c = {
		(uint8_t)(hex >> 16 & 0xff),
		(uint8_t)(hex >> 8 & 0xff),
		(uint8_t)(hex & 0xff),
		0xff,
	};
	return c;
}

static nui_color nui_rgba(uint32_t hex) {
	nui_color c = {
		(uint8_t)(hex >> 24 & 0xff),
		(uint8_t)(hex >> 16 & 0xff),
		(uint8_t)(hex >> 8 & 0xff),
		(uint8_t)(hex & 0xff),
	};
	return c;
}

nui_color nui_blend_over(nui_color src, nui_color dst);

// nui_point

static nui_point nui_pt(int32_t x, int32_t y) {
	nui_point p = { x, y };
	return p;
}

static int nui_point_eq(nui_point a, nui_point b) {
	return a.x == b.x && a.y == b.y;
}

static nui_point nui_offset(nui_point a, nui_point b) {
	nui_point p = { a.x + b.x, a.y + b.y };
	return p;
}

// nui_extent

static nui_extent nui_ex(int32_t x, int32_t y) {
	nui_extent ex = { x, y };
	return ex;
}

// nui_rect

static int32_t nui_width(const nui_rect *r) { return r->right - r->left; }
static int32_t nui_height(const nui_rect *r) { return r->bottom - r->top; }
static nui_extent nui_size(const nui_rect *r) {
	nui_extent e = { r->right - r->left, r->bottom - r->top };
}

static int nui_rect_eq(const nui_rect *a, const nui_rect *b) {
	return a->left == b->left && a->top == b->top
		&& a->right == b->right && a->bottom == b->bottom;
}

static int nui_intersects(const nui_rect *a, const nui_rect *b) {
	return !(a->left >= b->right || a->top >= b->bottom
		|| a->right <= b->left || a->bottom <= b->top);
}

// Allocations

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

#define nui_make(type) (type*)nui_alloc(sizeof(type))

#ifdef __cplusplus
}
#endif
