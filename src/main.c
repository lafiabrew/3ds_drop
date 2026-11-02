#include "drop_server.h"
#include "pk_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void render_ui(const drop_server_t *srv) {
    pk_render_begin_frame();
    pk_render_begin_target(PK_TARGET_TOP_LEFT, PK_RGBA8(14, 22, 36, 255));
    pk_draw_rect(0, 0, 400, 26, PK_RGBA8(30, 52, 88, 255));
    pk_draw_text(8, 7, 0.55f, PK_RGBA8(120, 240, 255, 255),
                 "3DS DROP - WIRELESS SD SCAN, LOG & SAVE SERVER");

    char line[DROP_PATH_MAX + 64];
    snprintf(line, sizeof(line), "3DS IP: %s:%u   SD: %s",
             srv->ip_addr, (unsigned)srv->port, srv->sd_root);
    pk_draw_text(10, 40, 0.52f, PK_RGBA8(255, 230, 95, 255), line);

    snprintf(line, sizeof(line), "REQUESTS: %u  |  SCANS: %u  |  PULLED: %u  |  PUSHED: %u",
             (unsigned)srv->requests_served, (unsigned)srv->scans_served,
             (unsigned)srv->files_pulled, (unsigned)srv->files_pushed);
    pk_draw_text(10, 62, 0.5f, PK_RGBA8(180, 235, 195, 255), line);

    pk_draw_rect(8, 86, 384, 110, PK_RGBA8(22, 32, 52, 255));
    pk_draw_rect_outline(8, 86, 384, 110, 1.0f, PK_RGBA8(75, 120, 190, 255));
    pk_draw_text(16, 96, 0.5f, PK_RGBA8(220, 230, 245, 255),
                 "ENDPOINTS:");
    pk_draw_text(16, 114, 0.45f, PK_RGBA8(170, 210, 250, 255),
                 "GET  /api/scan            - FULL SD INVENTORY JSON");
    pk_draw_text(16, 130, 0.45f, PK_RGBA8(170, 210, 250, 255),
                 "GET  /api/logs            - LUMA CRASH DUMPS & LOGS");
    pk_draw_text(16, 146, 0.45f, PK_RGBA8(170, 210, 250, 255),
                 "GET  /api/pull?path=...   - DOWNLOAD SAVE / LOG / SCREENSHOT");
    pk_draw_text(16, 162, 0.45f, PK_RGBA8(170, 210, 250, 255),
                 "POST /api/push?path=...   - ATOMIC UPLOAD (.BAK + RENAME)");

    pk_draw_rect(0, 216, 400, 24, PK_RGBA8(22, 32, 52, 255));
    pk_draw_text(8, 222, 0.5f, PK_RGBA8(120, 245, 170, 255), srv->last_event);

    pk_render_begin_target(PK_TARGET_BOTTOM, PK_RGBA8(14, 18, 30, 255));
    pk_draw_rect(12, 24, 296, 192, PK_RGBA8(26, 36, 58, 255));
    snprintf(line, sizeof(line), "3DS URL: http://%s:%u", srv->ip_addr, (unsigned)srv->port);
    pk_draw_text(20, 38, 0.55f, PK_RGBA8(110, 250, 175, 255), line);
    pk_draw_text(20, 64, 0.5f, PK_RGBA8(255, 230, 130, 255),
                 "PC COMPANION COMMAND:");
    pk_draw_text(20, 86, 0.45f, PK_RGBA8(210, 235, 255, 255),
                 "python3 pc_companion/sd_scanner.py \\");
    snprintf(line, sizeof(line), "  --url http://%s:%u", srv->ip_addr, (unsigned)srv->port);
    pk_draw_text(20, 104, 0.45f, PK_RGBA8(255, 240, 150, 255), line);
    pk_draw_text(20, 154, 0.5f, PK_RGBA8(170, 190, 220, 255),
                 "PRESS START TO EXIT");
    pk_render_end_frame();
}

int main(int argc, char **argv) {
    const char *sd_root = NULL;
    uint16_t port = DROP_DEFAULT_PORT;
    bool one_shot = false;
    bool daemon_mode = false;
    int max_requests = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sd-root") == 0 && i + 1 < argc) {
            sd_root = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--once") == 0) {
            one_shot = true;
        } else if (strcmp(argv[i], "--daemon") == 0) {
            daemon_mode = true;
        } else if (strcmp(argv[i], "--max-requests") == 0 && i + 1 < argc) {
            max_requests = atoi(argv[++i]);
        }
    }

    if (!daemon_mode && !pk_platform_init()) {
        return 1;
    }

    drop_server_t srv;
    if (!drop_server_init(&srv, sd_root != NULL ? sd_root : pk_platform_default_sd_root(), port)) {
        if (!daemon_mode) pk_platform_shutdown();
        return 1;
    }

    if (daemon_mode) {
        printf("3DS_DROP_LISTENING PORT=%u ROOT=%s\n", (unsigned)srv.port, srv.sd_root);
        fflush(stdout);
        while (max_requests <= 0 || (int)srv.requests_served < max_requests) {
            drop_server_poll_once(&srv, 100);
        }
        drop_server_shutdown(&srv);
        return 0;
    }

    while (pk_platform_main_loop()) {
        pk_input_t in;
        pk_platform_poll_input(&in);
        if (in.down & PK_BTN_START) {
            break;
        }
        drop_detect_local_ip(srv.ip_addr, sizeof(srv.ip_addr));
        drop_server_poll_once(&srv, 15);
        render_ui(&srv);
        if (one_shot) {
            break;
        }
    }

    drop_server_shutdown(&srv);
    pk_platform_shutdown();
    return 0;
}
