#include "nui_base.h"
#include <stdlib.h>
#include <string.h>

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
