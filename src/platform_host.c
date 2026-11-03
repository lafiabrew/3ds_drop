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


#include <ctype.h>
#include <stdlib.h>

static void get_glyph_5x7(char ch, uint8_t cols[5]) {
    unsigned char uc = (unsigned char)toupper((unsigned char)ch);
    switch (uc) {
    case 'A': cols[0] = 0x7E; cols[1] = 0x11; cols[2] = 0x11; cols[3] = 0x11; cols[4] = 0x7E; break;
    case 'B': cols[0] = 0x7F; cols[1] = 0x49; cols[2] = 0x49; cols[3] = 0x49; cols[4] = 0x36; break;
    case 'C': cols[0] = 0x3E; cols[1] = 0x41; cols[2] = 0x41; cols[3] = 0x41; cols[4] = 0x22; break;
    case 'D': cols[0] = 0x7F; cols[1] = 0x41; cols[2] = 0x41; cols[3] = 0x22; cols[4] = 0x1C; break;
    case 'E': cols[0] = 0x7F; cols[1] = 0x49; cols[2] = 0x49; cols[3] = 0x49; cols[4] = 0x41; break;
    case 'F': cols[0] = 0x7F; cols[1] = 0x09; cols[2] = 0x09; cols[3] = 0x09; cols[4] = 0x01; break;
    case 'G': cols[0] = 0x3E; cols[1] = 0x41; cols[2] = 0x49; cols[3] = 0x49; cols[4] = 0x7A; break;
    case 'H': cols[0] = 0x7F; cols[1] = 0x08; cols[2] = 0x08; cols[3] = 0x08; cols[4] = 0x7F; break;
    case 'I': cols[0] = 0x00; cols[1] = 0x41; cols[2] = 0x7F; cols[3] = 0x41; cols[4] = 0x00; break;
    case 'J': cols[0] = 0x20; cols[1] = 0x40; cols[2] = 0x41; cols[3] = 0x3F; cols[4] = 0x01; break;
    case 'K': cols[0] = 0x7F; cols[1] = 0x08; cols[2] = 0x14; cols[3] = 0x22; cols[4] = 0x41; break;
    case 'L': cols[0] = 0x7F; cols[1] = 0x40; cols[2] = 0x40; cols[3] = 0x40; cols[4] = 0x40; break;
    case 'M': cols[0] = 0x7F; cols[1] = 0x02; cols[2] = 0x0C; cols[3] = 0x02; cols[4] = 0x7F; break;
    case 'N': cols[0] = 0x7F; cols[1] = 0x04; cols[2] = 0x08; cols[3] = 0x10; cols[4] = 0x7F; break;
    case 'O': cols[0] = 0x3E; cols[1] = 0x41; cols[2] = 0x41; cols[3] = 0x41; cols[4] = 0x3E; break;
    case 'P': cols[0] = 0x7F; cols[1] = 0x09; cols[2] = 0x09; cols[3] = 0x09; cols[4] = 0x06; break;
    case 'Q': cols[0] = 0x3E; cols[1] = 0x41; cols[2] = 0x51; cols[3] = 0x21; cols[4] = 0x5E; break;
    case 'R': cols[0] = 0x7F; cols[1] = 0x09; cols[2] = 0x19; cols[3] = 0x29; cols[4] = 0x46; break;
    case 'S': cols[0] = 0x46; cols[1] = 0x49; cols[2] = 0x49; cols[3] = 0x49; cols[4] = 0x31; break;
    case 'T': cols[0] = 0x01; cols[1] = 0x01; cols[2] = 0x7F; cols[3] = 0x01; cols[4] = 0x01; break;
    case 'U': cols[0] = 0x3F; cols[1] = 0x40; cols[2] = 0x40; cols[3] = 0x40; cols[4] = 0x3F; break;
    case 'V': cols[0] = 0x1F; cols[1] = 0x20; cols[2] = 0x40; cols[3] = 0x20; cols[4] = 0x1F; break;
    case 'W': cols[0] = 0x3F; cols[1] = 0x40; cols[2] = 0x38; cols[3] = 0x40; cols[4] = 0x3F; break;
    case 'X': cols[0] = 0x63; cols[1] = 0x14; cols[2] = 0x08; cols[3] = 0x14; cols[4] = 0x63; break;
    case 'Y': cols[0] = 0x07; cols[1] = 0x08; cols[2] = 0x70; cols[3] = 0x08; cols[4] = 0x07; break;
    case 'Z': cols[0] = 0x61; cols[1] = 0x51; cols[2] = 0x49; cols[3] = 0x45; cols[4] = 0x43; break;
    case '0': cols[0] = 0x3E; cols[1] = 0x51; cols[2] = 0x49; cols[3] = 0x45; cols[4] = 0x3E; break;
    case '1': cols[0] = 0x00; cols[1] = 0x42; cols[2] = 0x7F; cols[3] = 0x40; cols[4] = 0x00; break;
    case '2': cols[0] = 0x42; cols[1] = 0x61; cols[2] = 0x51; cols[3] = 0x49; cols[4] = 0x46; break;
    case '3': cols[0] = 0x21; cols[1] = 0x41; cols[2] = 0x45; cols[3] = 0x4B; cols[4] = 0x31; break;
    case '4': cols[0] = 0x18; cols[1] = 0x14; cols[2] = 0x12; cols[3] = 0x7F; cols[4] = 0x10; break;
    case '5': cols[0] = 0x27; cols[1] = 0x45; cols[2] = 0x45; cols[3] = 0x45; cols[4] = 0x39; break;
    case '6': cols[0] = 0x3C; cols[1] = 0x4A; cols[2] = 0x49; cols[3] = 0x49; cols[4] = 0x30; break;
    case '7': cols[0] = 0x01; cols[1] = 0x71; cols[2] = 0x09; cols[3] = 0x05; cols[4] = 0x03; break;
    case '8': cols[0] = 0x36; cols[1] = 0x49; cols[2] = 0x49; cols[3] = 0x49; cols[4] = 0x36; break;
    case '9': cols[0] = 0x06; cols[1] = 0x49; cols[2] = 0x49; cols[3] = 0x29; cols[4] = 0x1E; break;
    case '-': cols[0] = 0x08; cols[1] = 0x08; cols[2] = 0x08; cols[3] = 0x08; cols[4] = 0x08; break;
    case '+': cols[0] = 0x08; cols[1] = 0x08; cols[2] = 0x3E; cols[3] = 0x08; cols[4] = 0x08; break;
    case ':': cols[0] = 0x00; cols[1] = 0x36; cols[2] = 0x36; cols[3] = 0x00; cols[4] = 0x00; break;
    case '.': cols[0] = 0x00; cols[1] = 0x60; cols[2] = 0x60; cols[3] = 0x00; cols[4] = 0x00; break;
    case ',': cols[0] = 0x00; cols[1] = 0x50; cols[2] = 0x30; cols[3] = 0x00; cols[4] = 0x00; break;
    case '/': cols[0] = 0x20; cols[1] = 0x10; cols[2] = 0x08; cols[3] = 0x04; cols[4] = 0x02; break;
    case '|': cols[0] = 0x00; cols[1] = 0x00; cols[2] = 0x7F; cols[3] = 0x00; cols[4] = 0x00; break;
    case '(': cols[0] = 0x00; cols[1] = 0x1C; cols[2] = 0x22; cols[3] = 0x41; cols[4] = 0x00; break;
    case ')': cols[0] = 0x00; cols[1] = 0x41; cols[2] = 0x22; cols[3] = 0x1C; cols[4] = 0x00; break;
    case '[': cols[0] = 0x00; cols[1] = 0x7F; cols[2] = 0x41; cols[3] = 0x41; cols[4] = 0x00; break;
    case ']': cols[0] = 0x00; cols[1] = 0x41; cols[2] = 0x41; cols[3] = 0x7F; cols[4] = 0x00; break;
    case '#': cols[0] = 0x14; cols[1] = 0x7F; cols[2] = 0x14; cols[3] = 0x7F; cols[4] = 0x14; break;
    case '$': cols[0] = 0x24; cols[1] = 0x2A; cols[2] = 0x7F; cols[3] = 0x2A; cols[4] = 0x12; break;
    case '%': cols[0] = 0x23; cols[1] = 0x13; cols[2] = 0x08; cols[3] = 0x64; cols[4] = 0x62; break;
    case '*': cols[0] = 0x14; cols[1] = 0x08; cols[2] = 0x3E; cols[3] = 0x08; cols[4] = 0x14; break;
    case '_': cols[0] = 0x40; cols[1] = 0x40; cols[2] = 0x40; cols[3] = 0x40; cols[4] = 0x40; break;
    case '=': cols[0] = 0x14; cols[1] = 0x14; cols[2] = 0x14; cols[3] = 0x14; cols[4] = 0x14; break;
    case ' ': cols[0] = 0x00; cols[1] = 0x00; cols[2] = 0x00; cols[3] = 0x00; cols[4] = 0x00; break;
    default:  cols[0] = 0x00; cols[1] = 0x3E; cols[2] = 0x22; cols[3] = 0x3E; cols[4] = 0x00; break;
    }
}

void pk_render_end_frame(void) {
    const char *dump_path = getenv("PK_DUMP_FRAMEBUFFER");
    if (dump_path != NULL && dump_path[0] != '\0') {
        FILE *fp = fopen(dump_path, "wb");
        if (fp != NULL) {
            fwrite(fb_top_l, sizeof(uint32_t), PK_TOP_WIDTH * PK_TOP_HEIGHT, fp);
            fwrite(fb_bot, sizeof(uint32_t), PK_BOT_WIDTH * PK_BOT_HEIGHT, fp);
            fclose(fp);
        }
    }
}


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
    if (text == NULL || text[0] == '\0') return;
    (void)scale;
    int cur_x = (int)x;
    int base_y = (int)y + 2;
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (cur_x + 5 >= g_w - 2) break;
        uint8_t cols[5];
        get_glyph_5x7(text[i], cols);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = cols[col];
            for (int row = 0; row < 7; row++) {
                if ((bits >> row) & 1u) {
                    int px = cur_x + col;
                    int py = base_y + row;
                    if (px >= 0 && px < g_w && py >= 0 && py < g_h) {
                        g_fb[py * g_w + px] = color;
                    }
                }
            }
        }
        cur_x += 6;
    }
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
