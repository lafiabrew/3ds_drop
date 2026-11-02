#ifdef __3DS__

#include "pk_platform.h"

#include <3ds.h>
#include <citro2d.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DROP_SOC_ALIGN 0x1000
#define DROP_SOC_BUFFERSIZE 0x100000

static u32 *g_soc_buffer = NULL;
static C3D_RenderTarget *g_top_left = NULL;
static C3D_RenderTarget *g_top_right = NULL;
static C3D_RenderTarget *g_bottom = NULL;
static C2D_TextBuf g_text_buf = NULL;
static float g_active_target_w = 400.0f;
static bool g_prev_touch = false;
static int g_last_tx = 0;
static int g_last_ty = 0;

static uint32_t map_keys(u32 k) {
    uint32_t out = 0;
    if (k & KEY_A) out |= PK_BTN_A;
    if (k & KEY_B) out |= PK_BTN_B;
    if (k & KEY_SELECT) out |= PK_BTN_SELECT;
    if (k & KEY_START) out |= PK_BTN_START;
    if (k & (KEY_DRIGHT | KEY_CPAD_RIGHT)) out |= PK_BTN_DRIGHT;
    if (k & (KEY_DLEFT | KEY_CPAD_LEFT)) out |= PK_BTN_DLEFT;
    if (k & (KEY_DUP | KEY_CPAD_UP)) out |= PK_BTN_DUP;
    if (k & (KEY_DDOWN | KEY_CPAD_DOWN)) out |= PK_BTN_DDOWN;
    if (k & KEY_R) out |= PK_BTN_R;
    if (k & KEY_L) out |= PK_BTN_L;
    if (k & KEY_X) out |= PK_BTN_X;
    if (k & KEY_Y) out |= PK_BTN_Y;
    return out;
}

bool pk_platform_init(void) {
    gfxInitDefault();
    gfxSet3D(false);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/drop", 0777);

    g_soc_buffer = (u32 *)memalign(DROP_SOC_ALIGN, DROP_SOC_BUFFERSIZE);
    if (g_soc_buffer != NULL) {
        if (socInit(g_soc_buffer, DROP_SOC_BUFFERSIZE) != 0) {
            free(g_soc_buffer);
            g_soc_buffer = NULL;
        }
    }

    g_top_left = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_top_right = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    g_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_text_buf = C2D_TextBufNew(4096);
    g_prev_touch = false;
    return g_top_left != NULL && g_bottom != NULL && g_text_buf != NULL;
}

void pk_platform_shutdown(void) {
    if (g_text_buf != NULL) {
        C2D_TextBufDelete(g_text_buf);
        g_text_buf = NULL;
    }
    C2D_Fini();
    C3D_Fini();
    if (g_soc_buffer != NULL) {
        socExit();
        free(g_soc_buffer);
        g_soc_buffer = NULL;
    }
    gfxExit();
}

bool pk_platform_main_loop(void) {
    return aptMainLoop();
}

void pk_platform_poll_input(pk_input_t *out) {
    if (out == NULL) return;
    memset(out, 0, sizeof(*out));
    hidScanInput();
    u32 kd = hidKeysDown();
    u32 kh = hidKeysHeld();
    u32 ku = hidKeysUp();
    out->down = map_keys(kd);
    out->held = map_keys(kh);
    out->up   = map_keys(ku);

    bool touching = (kh & KEY_TOUCH) != 0;
    out->touching = touching;
    out->touch_just_pressed = touching && !g_prev_touch;
    if (touching) {
        touchPosition tp;
        hidTouchRead(&tp);
        g_last_tx = (int)tp.px;
        g_last_ty = (int)tp.py;
    }
    out->touch_x = g_last_tx;
    out->touch_y = g_last_ty;
    g_prev_touch = touching;
    out->slider_3d = osGet3DSliderState();
}

const char *pk_platform_default_sd_root(void) {
    return "sdmc:";
}

void pk_render_begin_frame(void) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    if (g_text_buf != NULL) {
        C2D_TextBufClear(g_text_buf);
    }
}

void pk_render_begin_target(pk_render_target_t target, uint32_t clear_color) {
    C3D_RenderTarget *rt = g_top_left;
    g_active_target_w = (target == PK_TARGET_BOTTOM) ? 320.0f : 400.0f;
    if (target == PK_TARGET_TOP_RIGHT) rt = g_top_right;
    else if (target == PK_TARGET_BOTTOM) rt = g_bottom;
    if (rt != NULL) {
        C2D_TargetClear(rt, clear_color);
        C2D_SceneBegin(rt);
    }
}

void pk_render_end_frame(void) {
    C3D_FrameEnd(0);
}

void pk_draw_rect(float x, float y, float w, float h, uint32_t color) {
    if (w > 0.0f && h > 0.0f) {
        C2D_DrawRectSolid(x, y, 0.5f, w, h, color);
    }
}

void pk_draw_rect_outline(float x, float y, float w, float h, float t, uint32_t color) {
    if (w <= 0.0f || h <= 0.0f || t <= 0.0f) return;
    C2D_DrawRectSolid(x, y, 0.5f, w, t, color);
    C2D_DrawRectSolid(x, y + h - t, 0.5f, w, t, color);
    C2D_DrawRectSolid(x, y + t, 0.5f, t, h - 2.0f * t, color);
    C2D_DrawRectSolid(x + w - t, y + t, 0.5f, t, h - 2.0f * t, color);
}

void pk_draw_text(float x, float y, float scale, uint32_t color, const char *text) {
    if (text == NULL || text[0] == '\0' || g_text_buf == NULL) return;
    C2D_Text ct;
    if (C2D_TextParse(&ct, g_text_buf, text) != NULL) {
        C2D_TextOptimize(&ct);
        float sx = scale;
        float sy = scale;
        float avail_w = g_active_target_w - x - 6.0f;
        if (avail_w > 24.0f && ct.width * sx > avail_w) {
            sx = avail_w / ct.width;
            if (sx < 0.30f) sx = 0.30f;
        }
        /* Align vertical offset so 30px system font centers nicely inside 14..22px rows */
        float y_adj = y - (sy * 4.0f);
        C2D_DrawText(&ct, C2D_WithColor, x, y_adj, 0.5f, sx, sy, color);
    }
}

const uint32_t *pk_platform_host_get_framebuffer(pk_render_target_t target) {
    (void)target;
    return NULL;
}

void pk_platform_host_inject_input(const pk_input_t *input) {
    (void)input;
}

#else
typedef int pk_platform_3ds_unused_t;
#endif
