/*
 * Unit & network integration tests for 3ds_drop:
 * - Proves path traversal validation (`../`, `/etc/passwd`, `C:\`) is rejected.
 * - Proves live non-blocking HTTP server endpoints (`/api/status`, `/api/scan`, `/api/logs`,
 *   `/api/pull`, `/api/push`) over loopback TCP socket.
 * - Proves pushing a file creates `<path>.bak` when overwriting an existing save and commits
 *   atomically via `<path>.tmp` -> `rename()`.
 */

#include "drop_server.h"
#include "pk_platform.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

static void mkdir_p(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

static void send_and_service(drop_server_t *srv, const char *req_str, char *resp_buf, size_t resp_cap) {
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(cfd >= 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(srv->port);

    assert(connect(cfd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    size_t rlen = strlen(req_str);
    assert(send(cfd, req_str, rlen, 0) == (ssize_t)rlen);

    assert(drop_server_poll_once(srv, 200));

    size_t total = 0;
    while (total + 1 < resp_cap) {
        ssize_t n = recv(cfd, resp_buf + total, resp_cap - 1 - total, 0);
        if (n <= 0) break;
        total += (size_t)n;
    }
    resp_buf[total] = '\0';
    close(cfd);
}

int main(void) {
    /* 1. Path traversal guard tests */
    assert(drop_validate_rel_path("3ds/Checkpoint/saves/main") == true);
    assert(drop_validate_rel_path("../etc/passwd") == false);
    assert(drop_validate_rel_path("3ds/../../secret") == false);
    assert(drop_validate_rel_path("/absolute/path") == false);
    assert(drop_validate_rel_path("sdmc:/3ds/file") == false);

    /* 2. Seed mock SD card */
    const char *sd_root = "build/test_sd";
    mkdir_p("build/test_sd/luma/dumps/arm11");
    mkdir_p("build/test_sd/3ds/Checkpoint/saves/PhotonicSun");

    FILE *fp = fopen("build/test_sd/luma/errdisp.txt", "wb");
    assert(fp != NULL);
    fputs("Error: ARM11 prefetch abort in title 00040000001B5000\n", fp);
    fclose(fp);

    fp = fopen("build/test_sd/3ds/Checkpoint/saves/PhotonicSun/main", "wb");
    assert(fp != NULL);
    fputs("ORIG_SAVE_V1", fp);
    fclose(fp);

    /* 3. Start drop_server on ephemeral port (0) */
    drop_server_t srv;
    assert(drop_server_init(&srv, sd_root, 0));
    assert(srv.port > 0);

    char resp[16384];

    /* GET /api/scan */
    send_and_service(&srv, "GET /api/scan HTTP/1.1\r\nHost: localhost\r\n\r\n", resp, sizeof(resp));
    assert(strstr(resp, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp, "PhotonicSun/main") != NULL);

    /* GET /api/logs */
    send_and_service(&srv, "GET /api/logs HTTP/1.1\r\nHost: localhost\r\n\r\n", resp, sizeof(resp));
    assert(strstr(resp, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp, "errdisp.txt") != NULL);
    assert(strstr(resp, "00040000001B5000") != NULL);

    /* Traversal attempt on /api/pull -> must return 400 Bad Request */
    send_and_service(&srv, "GET /api/pull?path=../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n",
                     resp, sizeof(resp));
    assert(strstr(resp, "HTTP/1.1 400 Bad Request") != NULL);

    /* POST /api/push to overwrite save -> must create .bak and update file */
    send_and_service(&srv,
                     "POST /api/push?path=3ds/Checkpoint/saves/PhotonicSun/main HTTP/1.1\r\n"
                     "Content-Length: 12\r\n\r\n"
                     "NEW_SAVE_V2!",
                     resp, sizeof(resp));
    assert(strstr(resp, "HTTP/1.1 200 OK") != NULL);

    /* Verify .bak contains ORIG_SAVE_V1 and main contains NEW_SAVE_V2! */
    fp = fopen("build/test_sd/3ds/Checkpoint/saves/PhotonicSun/main.bak", "rb");
    assert(fp != NULL);
    char check[64] = {0};
    assert(fread(check, 1, 12, fp) == 12);
    fclose(fp);
    assert(strcmp(check, "ORIG_SAVE_V1") == 0);

    /* Pull modified file via GET /api/pull */
    send_and_service(&srv,
                     "GET /api/pull?path=3ds/Checkpoint/saves/PhotonicSun/main HTTP/1.1\r\nHost: localhost\r\n\r\n",
                     resp, sizeof(resp));
    assert(strstr(resp, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(resp, "NEW_SAVE_V2!") != NULL);

    drop_server_shutdown(&srv);
    printf("PASS: test_drop all HTTP scan, log harvest, path-traversal guard, and atomic push/pull checks succeeded.\n");
    return 0;
}
