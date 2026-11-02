#ifndef PK_PLATFORM_H
#define PK_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PK_TOP_WIDTH 400
#define PK_TOP_HEIGHT 240
#define PK_BOT_WIDTH 320
#define PK_BOT_HEIGHT 240

#define PK_BTN_A      (1u << 0)
#define PK_BTN_B      (1u << 1)
#define PK_BTN_SELECT (1u << 2)
#define PK_BTN_START  (1u << 3)
#define PK_BTN_DRIGHT (1u << 4)
#define PK_BTN_DLEFT  (1u << 5)
#define PK_BTN_DUP    (1u << 6)
#define PK_BTN_DDOWN  (1u << 7)
#define PK_BTN_R      (1u << 8)
#define PK_BTN_L      (1u << 9)
#define PK_BTN_X      (1u << 10)
#define PK_BTN_Y      (1u << 11)

typedef enum {
    PK_TARGET_TOP_LEFT = 0,
    PK_TARGET_TOP_RIGHT = 1,
    PK_TARGET_BOTTOM = 2
} pk_render_target_t;

typedef struct {
    uint32_t down;
    uint32_t held;
    uint32_t up;
    bool touching;
    bool touch_just_pressed;
    int touch_x;
    int touch_y;
    float slider_3d;
} pk_input_t;

#define PK_RGBA8(r, g, b, a) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))

bool pk_platform_init(void);
void pk_platform_shutdown(void);
bool pk_platform_main_loop(void);
void pk_platform_poll_input(pk_input_t *out_input);
const char *pk_platform_default_sd_root(void);

void pk_render_begin_frame(void);
void pk_render_begin_target(pk_render_target_t target, uint32_t clear_color);
void pk_render_end_frame(void);

void pk_draw_rect(float x, float y, float w, float h, uint32_t color);
void pk_draw_rect_outline(float x, float y, float w, float h, float thickness, uint32_t color);
void pk_draw_text(float x, float y, float scale, uint32_t color, const char *text);

const uint32_t *pk_platform_host_get_framebuffer(pk_render_target_t target);
void pk_platform_host_inject_input(const pk_input_t *input);

#endif /* PK_PLATFORM_H */
