#pragma once

typedef struct nui_renderer nui_renderer;
typedef struct nui_render_info nui_render_info;

#ifdef __cplusplus
extern "C" {
#endif

nui_renderer *nui_gdi_renderer_make(void *dc);

void nui_gdi_render(void *dc, const nui_render_info *ri);

#ifdef __cplusplus
}
#endif
