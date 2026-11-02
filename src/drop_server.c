#define _POSIX_C_SOURCE 200809L

#include "drop_server.h"
#include "pk_platform.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __3DS__
#include <3ds.h>
#endif

void drop_detect_local_ip(char *out_ip, size_t cap) {
    if (out_ip == NULL || cap < 8) return;
#ifdef __3DS__
    uint32_t hid = (uint32_t)gethostid();
    if (hid != 0 && hid != 0x7F000001u && hid != 0x0100007Fu) {
        struct in_addr ia;
        ia.s_addr = hid;
        const char *s = inet_ntoa(ia);
        if (s != NULL && s[0] != '\0') {
            snprintf(out_ip, cap, "%s", s);
            return;
        }
    }
    snprintf(out_ip, cap, "0.0.0.0 (WIFI OFF)");
#else
    int ufd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ufd >= 0) {
        struct sockaddr_in dst;
        memset(&dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(80);
        inet_pton(AF_INET, "10.255.255.255", &dst.sin_addr);
        if (connect(ufd, (struct sockaddr *)&dst, sizeof(dst)) == 0) {
            struct sockaddr_in local;
            socklen_t llen = sizeof(local);
            if (getsockname(ufd, (struct sockaddr *)&local, &llen) == 0) {
                const char *s = inet_ntoa(local.sin_addr);
                if (s != NULL && s[0] != '\0') {
                    snprintf(out_ip, cap, "%s", s);
                    close(ufd);
                    return;
                }
            }
        }
        close(ufd);
    }
    snprintf(out_ip, cap, "127.0.0.1");
#endif
}

bool drop_validate_rel_path(const char *rel_path) {
    if (rel_path == NULL || rel_path[0] == '\0') {
        return false;
    }
    if (rel_path[0] == '/' || rel_path[0] == '\\' || strchr(rel_path, ':') != NULL) {
        return false;
    }
    if (strstr(rel_path, "..") != NULL || strchr(rel_path, '\\') != NULL) {
        return false;
    }
    for (const char *p = rel_path; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c == '"' || c == '\'') {
            return false;
        }
    }
    return true;
}

static void append_fmt(char *buf, size_t cap, size_t *io_len, const char *fmt, ...) {
    if (*io_len + 1 >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *io_len, cap - *io_len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        size_t added = (size_t)n;
        if (*io_len + added >= cap) {
            *io_len = cap - 1;
        } else {
            *io_len += added;
        }
    }
}

static void ensure_parent_dirs(const char *full_path) {
    char tmp[DROP_PATH_MAX + 320];
    snprintf(tmp, sizeof(tmp), "%s", full_path);
    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
}

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    int count;
} json_list_ctx_t;

static void walk_sd_files(const char *sd_root, const char *rel_dir, int depth, json_list_ctx_t *ctx) {
    if (depth > 4 || ctx->count >= 128) return;
    char dir_path[DROP_PATH_MAX + 64];
    if (rel_dir[0] == '\0') {
        snprintf(dir_path, sizeof(dir_path), "%s", sd_root);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s/%s", sd_root, rel_dir);
    }
    DIR *d = opendir(dir_path);
    if (d == NULL) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && ctx->count < 128) {
        if (ent->d_name[0] == '.') continue;
        if (depth == 0 && (strcmp(ent->d_name, "Nintendo 3DS") == 0 ||
                           strcmp(ent->d_name, "System Volume Information") == 0 ||
                           strcmp(ent->d_name, "boot9strap") == 0)) {
            continue;
        }
        char rel_child[DROP_PATH_MAX];
        if (rel_dir[0] == '\0') {
            snprintf(rel_child, sizeof(rel_child), "%s", ent->d_name);
        } else {
            snprintf(rel_child, sizeof(rel_child), "%s/%s", rel_dir, ent->d_name);
        }
        char full[DROP_PATH_MAX + 64];
        snprintf(full, sizeof(full), "%s/%s", sd_root, rel_child);

        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk_sd_files(sd_root, rel_child, depth + 1, ctx);
        } else if (S_ISREG(st.st_mode)) {
            const char *category = "other";
            if (strstr(rel_child, "Checkpoint/saves") != NULL ||
                strstr(rel_child, ".sav") != NULL ||
                strcmp(ent->d_name, "main") == 0) {
                category = "save";
            } else if (strstr(rel_child, "luma/titles") != NULL) {
                category = "layeredfs_mod";
            } else if (strstr(rel_child, "luma/dumps") != NULL ||
                       strstr(rel_child, "errdisp.txt") != NULL ||
                       strstr(rel_child, ".log") != NULL) {
                category = "log";
            } else if (strstr(rel_child, "screenshots") != NULL) {
                category = "screenshot";
            }
            append_fmt(ctx->buf, ctx->cap, &ctx->len,
                       "%s{\"path\":\"%s\",\"size\":%zu,\"category\":\"%s\"}",
                       ctx->count > 0 ? "," : "", rel_child, (size_t)st.st_size, category);
            ctx->count++;
        }
    }
    closedir(d);
}

size_t drop_build_scan_json(const char *sd_root, char *out_buf, size_t cap) {
    if (sd_root == NULL || out_buf == NULL || cap < 64) return 0;
    out_buf[0] = '\0';
    json_list_ctx_t ctx;
    ctx.buf = out_buf;
    ctx.cap = cap;
    ctx.len = 0;
    ctx.count = 0;

    append_fmt(out_buf, cap, &ctx.len, "{\"status\":\"ok\",\"sd_root\":\"%s\",\"files\":[", sd_root);
    walk_sd_files(sd_root, "", 0, &ctx);
    append_fmt(out_buf, cap, &ctx.len, "],\"total_files\":%d}\n", ctx.count);
    return ctx.len;
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void walk_sd_logs(const char *sd_root, const char *rel_dir, int depth, json_list_ctx_t *ctx) {
    if (depth > 4 || ctx->count >= 32) return;
    char dir_path[DROP_PATH_MAX + 64];
    if (rel_dir[0] == '\0') {
        snprintf(dir_path, sizeof(dir_path), "%s", sd_root);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s/%s", sd_root, rel_dir);
    }
    DIR *d = opendir(dir_path);
    if (d == NULL) return;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && ctx->count < 32) {
        if (ent->d_name[0] == '.') continue;
        if (depth == 0 && (strcmp(ent->d_name, "Nintendo 3DS") == 0 ||
                           strcmp(ent->d_name, "System Volume Information") == 0 ||
                           strcmp(ent->d_name, "boot9strap") == 0)) {
            continue;
        }
        char rel_child[DROP_PATH_MAX];
        if (rel_dir[0] == '\0') {
            snprintf(rel_child, sizeof(rel_child), "%s", ent->d_name);
        } else {
            snprintf(rel_child, sizeof(rel_child), "%s/%s", rel_dir, ent->d_name);
        }
        char full[DROP_PATH_MAX + 64];
        snprintf(full, sizeof(full), "%s/%s", sd_root, rel_child);

        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk_sd_logs(sd_root, rel_child, depth + 1, ctx);
        } else if (S_ISREG(st.st_mode)) {
            size_t nl = strlen(ent->d_name);
            bool is_dmp = (nl >= 4 && strcmp(ent->d_name + nl - 4, ".dmp") == 0);
            bool is_log = (nl >= 4 && strcmp(ent->d_name + nl - 4, ".log") == 0) ||
                          strcmp(ent->d_name, "errdisp.txt") == 0 ||
                          strcmp(ent->d_name, "crash.txt") == 0 ||
                          strcmp(ent->d_name, "diagnostic_snapshot.json") == 0;
            if (!is_dmp && !is_log) continue;

            FILE *fp = fopen(full, "rb");
            if (fp == NULL) continue;
            uint8_t raw[256];
            memset(raw, 0, sizeof(raw));
            size_t rlen = fread(raw, 1, sizeof(raw) - 1, fp);
            fclose(fp);

            char snippet[200];
            if (is_dmp && rlen >= 24) {
                uint32_t magic = read_u32_le(raw + 0x00);
                uint32_t proc  = read_u32_le(raw + 0x08);
                uint32_t exc   = read_u32_le(raw + 0x0C);
                uint32_t pc    = read_u32_le(raw + 0x10);
                uint32_t lr    = read_u32_le(raw + 0x14);
                snprintf(snippet, sizeof(snippet),
                         "LUMA_DMP HDR:%08X ARM%u EXC:%u PC:0x%08X LR:0x%08X",
                         (unsigned)magic, (unsigned)proc, (unsigned)exc,
                         (unsigned)pc, (unsigned)lr);
            } else {
                size_t si = 0;
                for (size_t i = 0; i < rlen && si + 1 < sizeof(snippet); i++) {
                    char c = (char)raw[i];
                    if (c == '\r' || c == '\n') {
                        if (si > 0 && snippet[si - 1] != ' ') snippet[si++] = ' ';
                    } else if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
                        snippet[si++] = c;
                    }
                }
                snippet[si] = '\0';
            }

            append_fmt(ctx->buf, ctx->cap, &ctx->len,
                       "%s{\"path\":\"%s\",\"size\":%zu,\"type\":\"%s\",\"excerpt\":\"%s\"}",
                       ctx->count > 0 ? "," : "", rel_child, (size_t)st.st_size,
                       is_dmp ? "crash_dump" : "log", snippet);
            ctx->count++;
        }
    }
    closedir(d);
}

size_t drop_build_logs_json(const char *sd_root, char *out_buf, size_t cap) {
    if (sd_root == NULL || out_buf == NULL || cap < 64) return 0;
    out_buf[0] = '\0';
    json_list_ctx_t ctx;
    ctx.buf = out_buf;
    ctx.cap = cap;
    ctx.len = 0;
    ctx.count = 0;

    append_fmt(out_buf, cap, &ctx.len, "{\"status\":\"ok\",\"logs\":[");
    walk_sd_logs(sd_root, "", 0, &ctx);
    append_fmt(out_buf, cap, &ctx.len, "],\"log_count\":%d}\n", ctx.count);
    return ctx.len;
}

static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool drop_server_init(drop_server_t *srv, const char *sd_root, uint16_t port) {
    if (srv == NULL) return false;
    memset(srv, 0, sizeof(*srv));
    srv->listen_fd = -1;
    snprintf(srv->sd_root, sizeof(srv->sd_root), "%s", sd_root != NULL ? sd_root : "./sdmc");

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(srv->last_event, sizeof(srv->last_event), "SOCKET INIT FAILED");
        return false;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    set_nonblocking(fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        snprintf(srv->last_event, sizeof(srv->last_event), "BIND PORT %u FAILED", (unsigned)port);
        return false;
    }
    if (listen(fd, 4) != 0) {
        close(fd);
        return false;
    }

    socklen_t alen = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &alen) == 0) {
        srv->port = ntohs(addr.sin_port);
    } else {
        srv->port = port;
    }
    srv->listen_fd = fd;
    drop_detect_local_ip(srv->ip_addr, sizeof(srv->ip_addr));
    snprintf(srv->last_event, sizeof(srv->last_event),
             "READY AT HTTP://%s:%u", srv->ip_addr, (unsigned)srv->port);
    pk_log_event(srv->sd_root, "drop", "INFO", srv->last_event);
    return true;
}

static bool send_all_bounded(int fd, const uint8_t *data, size_t len, int timeout_ms) {
    size_t sent = 0;
    while (sent < len) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, timeout_ms);
        if (pr <= 0) return false;
        size_t chunk = len - sent;
        if (chunk > 1024u) chunk = 1024u;
        ssize_t n = send(fd, data + sent, chunk, 0);
        if (n <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return false;
        }
        sent += (size_t)n;
    }
    return true;
}

static void graceful_close_client(int cfd) {
    shutdown(cfd, SHUT_WR);
    uint8_t drain[128];
    for (int i = 0; i < 6; i++) {
        struct pollfd pfd;
        pfd.fd = cfd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        if (poll(&pfd, 1, 20) <= 0) break;
        if (recv(cfd, drain, sizeof(drain), 0) <= 0) break;
    }
    close(cfd);
}

static void send_http_response(int fd, int status, const char *status_text,
                               const char *content_type, const uint8_t *body, size_t body_len) {
    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n\r\n",
                        status, status_text, content_type, body_len);
    if (hlen < 0) return;
    size_t hdr_len = (size_t)hlen;
    if (body != NULL && body_len > 0) {
        size_t tot = hdr_len + body_len;
        uint8_t *combined = (uint8_t *)malloc(tot);
        if (combined != NULL) {
            memcpy(combined, hdr, hdr_len);
            memcpy(combined + hdr_len, body, body_len);
            send_all_bounded(fd, combined, tot, 2000);
            free(combined);
            return;
        }
    }
    send_all_bounded(fd, (const uint8_t *)hdr, hdr_len, 1000);
}

static void extract_query_param(const char *uri, const char *key, char *out_val, size_t cap) {
    out_val[0] = '\0';
    const char *q = strchr(uri, '?');
    if (q == NULL) return;
    q++;
    size_t klen = strlen(key);
    while (*q != '\0') {
        if (strncmp(q, key, klen) == 0 && q[klen] == '=') {
            const char *v = q + klen + 1;
            size_t oi = 0;
            while (*v != '\0' && *v != '&' && *v != ' ' && oi + 1 < cap) {
                if (v[0] == '%' && v[1] != '\0' && v[2] != '\0') {
                    char hex[3] = { v[1], v[2], '\0' };
                    out_val[oi++] = (char)strtol(hex, NULL, 16);
                    v += 3;
                } else {
                    out_val[oi++] = *v++;
                }
            }
            out_val[oi] = '\0';
            return;
        }
        const char *amp = strchr(q, '&');
        if (amp == NULL) break;
        q = amp + 1;
    }
}

bool drop_server_poll_once(drop_server_t *srv, int timeout_ms) {
    if (srv == NULL || srv->listen_fd < 0) return false;
    struct pollfd pfd;
    pfd.fd = srv->listen_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0 || !(pfd.revents & POLLIN)) {
        return false;
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int cfd = accept(srv->listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
    if (cfd < 0) return false;
    set_nonblocking(cfd);

    /* Read HTTP request headers + optional body (bounded up to 1 MB for Gen 3-7 save/log drops) */
    size_t cap = 1048576u;
    uint8_t *req = (uint8_t *)malloc(cap);
    if (req == NULL) {
        close(cfd);
        return false;
    }
    size_t total = 0;
    size_t header_end = 0;
    size_t expected_body = 0;

    for (int iter = 0; iter < 50 && total + 1 < cap; iter++) {
        struct pollfd cp;
        cp.fd = cfd;
        cp.events = POLLIN;
        cp.revents = 0;
        if (poll(&cp, 1, 25) <= 0) {
            if (header_end > 0 && total >= header_end + expected_body) break;
            continue;
        }
        ssize_t n = recv(cfd, req + total, cap - 1 - total, 0);
        if (n <= 0) break;
        total += (size_t)n;
        req[total] = '\0';

        if (header_end == 0) {
            char *hend = strstr((char *)req, "\r\n\r\n");
            if (hend != NULL) {
                header_end = (size_t)(hend - (char *)req) + 4u;
                char *cl = strstr((char *)req, "Content-Length:");
                if (cl == NULL) cl = strstr((char *)req, "content-length:");
                if (cl != NULL && cl < hend) {
                    expected_body = (size_t)strtoul(cl + 15, NULL, 10);
                }
            }
        }
        if (header_end > 0 && total >= header_end + expected_body) {
            break;
        }
    }

    char method[16] = {0};
    char uri[512] = {0};
    if (total > 0) {
        sscanf((char *)req, "%15s %511s", method, uri);
    }

    srv->requests_served++;

    if (strcmp(uri, "/") == 0 || strncmp(uri, "/?", 2) == 0 || strncmp(uri, "/index.html", 11) == 0) {
        static const char k_html[] =
            "<!doctype html><html><head><meta charset=\"utf-8\"/>"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"/>"
            "<title>3DS Drop — Wireless SD Manager</title>"
            "<style>"
            "body{background:#0d1424;color:#eef3fc;font-family:system-ui,sans-serif;margin:0;padding:1.5rem;max-width:980px;margin:0 auto}"
            "h1{color:#66e3ff;margin:0 0 .3rem 0}.sub{color:#9ab0d4;margin:0 0 1.2rem 0;font-size:.9rem}"
            ".grid{display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-bottom:1rem}"
            "@media(max-width:700px){.grid{grid-template-columns:1fr}}"
            ".card{background:#16223a;border:1px solid #2b426e;border-radius:12px;padding:1rem}"
            "h2{margin:0 0 .6rem 0;font-size:1.05rem;color:#ffd86b}"
            "input,button{padding:.48rem .7rem;border-radius:7px;border:1px solid #355084;background:#0b1120;color:#fff;font-size:.86rem}"
            "button{background:#38c890;color:#08150f;font-weight:700;border:none;cursor:pointer}"
            "button.sec{background:#34548a;color:#fff}"
            "table{width:100%;border-collapse:collapse;font-size:.84rem;margin-top:.5rem}"
            "th,td{text-align:left;padding:.4rem .5rem;border-bottom:1px solid #223456}"
            "a.dl{color:#66e3ff;text-decoration:none;font-weight:600}"
            "pre{background:#090e1a;padding:.6rem;border-radius:7px;overflow-x:auto;font-size:.78rem;color:#b8d4ff;max-height:220px}"
            ".tag{display:inline-block;padding:.1rem .45rem;border-radius:99px;font-size:.72rem;background:#243b63;color:#9ad1ff}"
            "</style></head><body>"
            "<h1>3DS Drop — Wireless SD Card &amp; Log Dashboard</h1>"
            "<p class=\"sub\" id=\"status\">Connecting to 3DS...</p>"
            "<div class=\"grid\">"
            "<div class=\"card\"><h2>Upload Save / ROM / LayeredFS File</h2>"
            "<p style=\"font-size:.8rem;color:#9ab0d4;margin:0 0 .5rem 0\">Existing files are automatically backed up to <code>.bak</code> before atomic overwrite.</p>"
            "<div style=\"display:flex;flex-direction:column;gap:.5rem\">"
            "<input type=\"file\" id=\"upFile\"/>"
            "<input type=\"text\" id=\"upPath\" placeholder=\"SD path (e.g. 3ds/Checkpoint/saves/main or roms/gba/hack.sav)\"/>"
            "<button onclick=\"uploadFile()\">Upload Atomically to 3DS SD</button>"
            "<div id=\"upMsg\" style=\"font-size:.82rem;color:#66ffaa\"></div>"
            "</div></div>"
            "<div class=\"card\"><h2>Luma3DS Crash Dumps &amp; Diagnostic Logs</h2>"
            "<button class=\"sec\" onclick=\"refreshAll()\">Refresh SD &amp; Logs</button>"
            "<pre id=\"logsBox\">Loading logs...</pre></div>"
            "</div>"
            "<div class=\"card\"><h2>SD Card Files (Saves, LayeredFS Mods, Logs, Screenshots)</h2>"
            "<table><thead><tr><th>Category</th><th>SD Relative Path</th><th>Size</th><th>Action</th></tr></thead>"
            "<tbody id=\"fileRows\"></tbody></table></div>"
            "<script>"
            "document.getElementById('upFile').addEventListener('change',e=>{"
            "const f=e.target.files[0];if(f&&!document.getElementById('upPath').value){"
            "document.getElementById('upPath').value='3ds/drop/'+f.name;}});"
            "async function refreshAll(){"
            "const st=await (await fetch('/api/status')).json();"
            "document.getElementById('status').textContent=`Connected to 3DS (${st.ip}:${st.port}) | SD Root: ${st.sd_root} | Requests: ${st.requests}`;"
            "const sc=await (await fetch('/api/scan')).json();"
            "const tb=document.getElementById('fileRows');tb.innerHTML='';"
            "(sc.files||[]).forEach(f=>{"
            "const tr=document.createElement('tr');"
            "const u='/api/pull?path='+encodeURIComponent(f.path);"
            "tr.innerHTML=`<td><span class='tag'>${f.category}</span></td><td><code>${f.path}</code></td><td>${f.size} B</td>"
            "<td><a class='dl' href='${u}' download>Download</a> | <a class='dl' href='#' onclick=\"document.getElementById('upPath').value='${f.path}';return false;\">Target Overwrite</a></td>`;"
            "tb.appendChild(tr);});"
            "const lg=await (await fetch('/api/logs')).json();"
            "document.getElementById('logsBox').textContent=(lg.logs||[]).map(l=>`[${l.type}] ${l.path} (${l.size} B)\\n  ${l.excerpt}`).join('\\n\\n')||'No crash dumps or logs found.';"
            "}"
            "async function uploadFile(){"
            "const f=document.getElementById('upFile').files[0];"
            "const p=document.getElementById('upPath').value.trim();"
            "const m=document.getElementById('upMsg');"
            "if(!f||!p){m.textContent='Choose a file and target SD path first.';return;}"
            "m.textContent='Uploading...';"
            "const buf=await f.arrayBuffer();"
            "const r=await fetch('/api/push?path='+encodeURIComponent(p),{method:'POST',body:buf});"
            "const j=await r.json();"
            "m.textContent=r.ok?`Saved ${j.path} (${j.bytes} B, .bak backup created!)`:`Error: ${j.error}`;"
            "refreshAll();}"
            "refreshAll();"
            "</script></body></html>\n";
        send_http_response(cfd, 200, "OK", "text/html; charset=utf-8",
                           (const uint8_t *)k_html, sizeof(k_html) - 1u);
        snprintf(srv->last_event, sizeof(srv->last_event), "SERVED WEB UI DASHBOARD");
    } else if (strncmp(uri, "/api/status", 11) == 0) {
        drop_detect_local_ip(srv->ip_addr, sizeof(srv->ip_addr));
        char body[320];
        int blen = snprintf(body, sizeof(body),
                            "{\"status\":\"ok\",\"app\":\"3ds_drop\",\"ip\":\"%s\",\"port\":%u,\"sd_root\":\"%.64s\",\"requests\":%u}\n",
                            srv->ip_addr, (unsigned)srv->port, srv->sd_root, (unsigned)srv->requests_served);
        send_http_response(cfd, 200, "OK", "application/json", (const uint8_t *)body, (size_t)blen);
    } else if (strncmp(uri, "/api/scan", 9) == 0) {
        size_t jcap = 32768u;
        char *jbuf = (char *)malloc(jcap);
        if (jbuf != NULL) {
            size_t jlen = drop_build_scan_json(srv->sd_root, jbuf, jcap);
            send_http_response(cfd, 200, "OK", "application/json", (const uint8_t *)jbuf, jlen);
            free(jbuf);
            srv->scans_served++;
            snprintf(srv->last_event, sizeof(srv->last_event), "SERVED /API/SCAN");
        }
    } else if (strncmp(uri, "/api/logs", 9) == 0) {
        size_t jcap = 32768u;
        char *jbuf = (char *)malloc(jcap);
        if (jbuf != NULL) {
            size_t jlen = drop_build_logs_json(srv->sd_root, jbuf, jcap);
            send_http_response(cfd, 200, "OK", "application/json", (const uint8_t *)jbuf, jlen);
            free(jbuf);
            snprintf(srv->last_event, sizeof(srv->last_event), "SERVED /API/LOGS");
        }
    } else if (strncmp(uri, "/api/pull", 9) == 0) {
        char rel[256];
        extract_query_param(uri, "path", rel, sizeof(rel));
        if (!drop_validate_rel_path(rel)) {
            const char *err = "{\"error\":\"invalid_or_traversal_path\"}\n";
            send_http_response(cfd, 400, "Bad Request", "application/json",
                               (const uint8_t *)err, strlen(err));
        } else {
            char full[DROP_PATH_MAX + 260];
            snprintf(full, sizeof(full), "%s/%s", srv->sd_root, rel);
            FILE *fp = fopen(full, "rb");
            if (fp == NULL) {
                const char *err = "{\"error\":\"not_found\"}\n";
                send_http_response(cfd, 404, "Not Found", "application/json",
                                   (const uint8_t *)err, strlen(err));
            } else {
                fseek(fp, 0, SEEK_END);
                long fsz = ftell(fp);
                rewind(fp);
                if (fsz >= 0 && fsz <= 0x100000L) {
                    uint8_t *fbuf = (uint8_t *)malloc((size_t)fsz + 1u);
                    size_t rd = fread(fbuf, 1, (size_t)fsz, fp);
                    send_http_response(cfd, 200, "OK", "application/octet-stream", fbuf, rd);
                    free(fbuf);
                    srv->files_pulled++;
                    snprintf(srv->last_event, sizeof(srv->last_event), "PULLED %.64s (%ld B)", rel, fsz);
                    pk_log_event(srv->sd_root, "drop", "INFO", srv->last_event);
                }
                fclose(fp);
            }
        }
    } else if (strcmp(method, "POST") == 0 && strncmp(uri, "/api/push", 9) == 0) {
        char rel[256];
        extract_query_param(uri, "path", rel, sizeof(rel));
        if (!drop_validate_rel_path(rel) || header_end == 0) {
            const char *err = "{\"error\":\"invalid_or_traversal_path\"}\n";
            send_http_response(cfd, 400, "Bad Request", "application/json",
                               (const uint8_t *)err, strlen(err));
        } else {
            char full[DROP_PATH_MAX + 260];
            char bak[DROP_PATH_MAX + 270];
            char tmp[DROP_PATH_MAX + 270];
            snprintf(full, sizeof(full), "%s/%s", srv->sd_root, rel);
            snprintf(bak, sizeof(bak), "%s.bak", full);
            snprintf(tmp, sizeof(tmp), "%s.tmp", full);
            ensure_parent_dirs(full);

            /* If file already exists, copy it to <full>.bak before overwriting */
            FILE *existing = fopen(full, "rb");
            if (existing != NULL) {
                FILE *bfp = fopen(bak, "wb");
                if (bfp != NULL) {
                    uint8_t cbuf[4096];
                    size_t cn;
                    while ((cn = fread(cbuf, 1, sizeof(cbuf), existing)) > 0) {
                        fwrite(cbuf, 1, cn, bfp);
                    }
                    fflush(bfp);
                    fclose(bfp);
                }
                fclose(existing);
            }

            const uint8_t *body_ptr = req + header_end;
            size_t body_len = total >= header_end ? (total - header_end) : 0;
            FILE *tfp = fopen(tmp, "wb");
            bool ok = false;
            if (tfp != NULL) {
                size_t w = fwrite(body_ptr, 1, body_len, tfp);
                fflush(tfp);
                fclose(tfp);
                if (w == body_len && rename(tmp, full) == 0) {
                    ok = true;
                } else {
                    remove(tmp);
                }
            }
            if (ok) {
                srv->files_pushed++;
                snprintf(srv->last_event, sizeof(srv->last_event), "PUSHED %.64s (%zu B)", rel, body_len);
                pk_log_event(srv->sd_root, "drop", "INFO", srv->last_event);
                char resp[320];
                int rlen = snprintf(resp, sizeof(resp),
                                    "{\"status\":\"ok\",\"path\":\"%s\",\"bytes\":%zu}\n",
                                    rel, body_len);
                send_http_response(cfd, 200, "OK", "application/json",
                                   (const uint8_t *)resp, (size_t)rlen);
            } else {
                const char *err = "{\"error\":\"write_failed\"}\n";
                send_http_response(cfd, 500, "Internal Server Error", "application/json",
                                   (const uint8_t *)err, strlen(err));
            }
        }
    } else {
        const char *err = "{\"error\":\"unknown_endpoint\"}\n";
        send_http_response(cfd, 404, "Not Found", "application/json",
                           (const uint8_t *)err, strlen(err));
    }

    free(req);
    graceful_close_client(cfd);
    return true;
}

void drop_server_shutdown(drop_server_t *srv) {
    if (srv == NULL) return;
    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }
}
