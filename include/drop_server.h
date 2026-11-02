#ifndef DROP_SERVER_H
#define DROP_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DROP_DEFAULT_PORT 8766
#define DROP_PATH_MAX 512

typedef struct {
    char sd_root[DROP_PATH_MAX];
    char ip_addr[32];
    uint16_t port;
    int listen_fd;
    uint32_t requests_served;
    uint32_t files_pushed;
    uint32_t files_pulled;
    uint32_t scans_served;
    char last_event[DROP_PATH_MAX + 128];
} drop_server_t;

/* Query the active 3DS or host LAN IPv4 address (e.g. "192.168.0.150") */
void drop_detect_local_ip(char *out_ip, size_t cap);

/* Validate a relative SD card path, rejecting any '..' traversal or leading slashes */
bool drop_validate_rel_path(const char *rel_path);

/* Build full SD card inventory JSON (saves, ROM hacks, Luma LayeredFS mods, logs, screenshots) */
size_t drop_build_scan_json(const char *sd_root, char *out_buf, size_t cap);

/* Build full diagnostic logs JSON (contents/summaries of errdisp.txt, .dmp headers, and .log files) */
size_t drop_build_logs_json(const char *sd_root, char *out_buf, size_t cap);

/* Start non-blocking TCP HTTP server on `port` (pass 0 to let OS pick an ephemeral port) */
bool drop_server_init(drop_server_t *srv, const char *sd_root, uint16_t port);

/* Poll and service at most one HTTP request with bounded non-blocking timeout (`timeout_ms` <= 250ms) */
bool drop_server_poll_once(drop_server_t *srv, int timeout_ms);

void drop_server_shutdown(drop_server_t *srv);

#endif /* DROP_SERVER_H */
