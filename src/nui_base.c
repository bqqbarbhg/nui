#include "nui_base.h"
#include <stdlib.h>
#include <string.h>

// nui_color

nui_color nui_blend_over(nui_color src, nui_color dst)
{
	uint32_t a = dst.a;
	uint32_t ia = 255 - a;
	nui_color col = {
		(uint8_t)(((uint32_t)src.r * ia + (uint32_t)dst.r * a) / 255),
		(uint8_t)(((uint32_t)src.g * ia + (uint32_t)dst.g * a) / 255),
		(uint8_t)(((uint32_t)src.b * ia + (uint32_t)dst.b * a) / 255),
		src.a, // TODO
	};
	return col;
}

// Allocations

#define NUI_MIN_ALLOC 128

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
