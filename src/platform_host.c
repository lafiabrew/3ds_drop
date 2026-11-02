#ifndef __3DS__

#include "pk_platform.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static uint32_t fb_top_l[PK_TOP_WIDTH * PK_TOP_HEIGHT];
static uint32_t fb_top_r[PK_TOP_WIDTH * PK_TOP_HEIGHT];
static uint32_t fb_bot[PK_BOT_WIDTH * PK_BOT_HEIGHT];

static uint32_t *g_fb = fb_top_l;
static int g_w = PK_TOP_WIDTH;
static int g_h = PK_TOP_HEIGHT;
static pk_input_t g_injected;

bool pk_platform_init(void) {
    memset(fb_top_l, 0, sizeof(fb_top_l));
    memset(fb_top_r, 0, sizeof(fb_top_r));
    memset(fb_bot, 0, sizeof(fb_bot));
    memset(&g_injected, 0, sizeof(g_injected));
    return true;
}

void pk_platform_shutdown(void) {}

bool pk_platform_main_loop(void) {
    return true;
}

void pk_platform_poll_input(pk_input_t *out) {
    if (out != NULL) {
        *out = g_injected;
    }
    memset(&g_injected, 0, sizeof(g_injected));
}

const char *pk_platform_default_sd_root(void) {
    return "./sdmc";
}

void pk_render_begin_frame(void) {
    g_fb = fb_top_l;
    g_w = PK_TOP_WIDTH;
    g_h = PK_TOP_HEIGHT;
}

void pk_render_begin_target(pk_render_target_t target, uint32_t clear_color) {
    if (target == PK_TARGET_TOP_LEFT) {
        g_fb = fb_top_l;
        g_w = PK_TOP_WIDTH;
        g_h = PK_TOP_HEIGHT;
    } else if (target == PK_TARGET_TOP_RIGHT) {
        g_fb = fb_top_r;
        g_w = PK_TOP_WIDTH;
        g_h = PK_TOP_HEIGHT;
    } else {
        g_fb = fb_bot;
        g_w = PK_BOT_WIDTH;
        g_h = PK_BOT_HEIGHT;
    }
    for (int i = 0; i < g_w * g_h; i++) {
        g_fb[i] = clear_color;
    }
}

void pk_render_end_frame(void) {}

void pk_draw_rect(float x, float y, float w, float h, uint32_t color) {
    int x0 = (int)x < 0 ? 0 : (int)x;
    int y0 = (int)y < 0 ? 0 : (int)y;
    int x1 = (int)(x + w) > g_w ? g_w : (int)(x + w);
    int y1 = (int)(y + h) > g_h ? g_h : (int)(y + h);
    for (int py = y0; py < y1; py++) {
        for (int px = x0; px < x1; px++) {
            g_fb[py * g_w + px] = color;
        }
    }
}

void pk_draw_rect_outline(float x, float y, float w, float h, float t, uint32_t color) {
    pk_draw_rect(x, y, w, t, color);
    pk_draw_rect(x, y + h - t, w, t, color);
    pk_draw_rect(x, y, t, h, color);
    pk_draw_rect(x + w - t, y, t, h, color);
}

void pk_draw_text(float x, float y, float scale, uint32_t color, const char *text) {
    if (text == NULL) return;
    int len = (int)strlen(text);
    int w = (int)(len * 6.0f * scale);
    int h = (int)(8.0f * scale);
    pk_draw_rect(x, y, (float)(w > 0 ? w : 4), (float)(h > 0 ? h : 4), color);
}


#include <time.h>

static void pk_ensure_dir_p(const char *dir_path) {
    char tmp[640];
    snprintf(tmp, sizeof(tmp), "%s", dir_path);
    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

bool pk_log_event(const char *sd_root, const char *app_name, const char *level, const char *msg) {
    if (sd_root == NULL || app_name == NULL || msg == NULL) return false;
    if (strstr(app_name, "..") != NULL || strchr(app_name, '/') != NULL) return false;

    char log_dir[576];
    snprintf(log_dir, sizeof(log_dir), "%s/3ds/logs", sd_root);
    pk_ensure_dir_p(log_dir);

    char log_path[640];
    snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir, app_name);
    FILE *fp = fopen(log_path, "ab");
    if (fp == NULL) return false;

    char ts[24];
    time_t now = time(NULL);
    struct tm *tm_utc = gmtime(&now);
    if (tm_utc != NULL && tm_utc->tm_year >= 120 &&
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_utc) == 19) {
        /* use real UTC clock */
    } else {
        memcpy(ts, "2026-11-02 09:00:00", 20);
    }

    fprintf(fp, "[%s] [%s] [%s] %s\n", ts, app_name, level != NULL ? level : "INFO", msg);
    fflush(fp);
    fclose(fp);
    return true;
}

void pk_draw_scrollbar(float x, float y, float h, int total_items, int visible_rows, int scroll_offset, uint32_t track_col, uint32_t thumb_col) {
    pk_draw_rect(x, y, 4.0f, h, track_col);
    if (total_items <= visible_rows || total_items <= 0) {
        pk_draw_rect(x, y, 4.0f, h, thumb_col);
        return;
    }
    float thumb_h = (h * (float)visible_rows) / (float)total_items;
    if (thumb_h < 12.0f) thumb_h = 12.0f;
    if (thumb_h > h) thumb_h = h;
    int max_scroll = total_items - visible_rows;
    float frac = max_scroll > 0 ? ((float)scroll_offset / (float)max_scroll) : 0.0f;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    float thumb_y = y + frac * (h - thumb_h);
    pk_draw_rect(x, thumb_y, 4.0f, thumb_h, thumb_col);
}

const uint32_t *pk_platform_host_get_framebuffer(pk_render_target_t target) {
    if (target == PK_TARGET_TOP_LEFT) return fb_top_l;
    if (target == PK_TARGET_TOP_RIGHT) return fb_top_r;
    return fb_bot;
}

void pk_platform_host_inject_input(const pk_input_t *input) {
    if (input != NULL) {
        g_injected = *input;
    }
}

#else
typedef int pk_platform_host_unused_t;
#endif
