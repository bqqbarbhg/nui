#pragma once

#include "nui_canvas.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void nui_gdi_render(HDC dc, const nui_render_info *ri);

#ifdef __cplusplus
}
#endif
