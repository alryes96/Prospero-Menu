#include "http_client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SHADOWMOUNT_PORT 10101
#define HTTP_RESPONSE_MAX (600u * 1024u)

static int send_all(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int find_header_end(const char *buf, size_t n, size_t *offset) {
    for (size_t i = 0; i + 3 < n; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            *offset = i + 4; return 0;
        }
    }
    return -1;
}

/* Small chunked-transfer decoder; ShadowMount normally returns Content-Length,
 * but the client accepts either response form. */
static int decode_chunked(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    size_t in = 0, out = 0;
    while (in < src_len) {
        char sizebuf[32]; size_t s = 0;
        while (in < src_len && src[in] != '\r' && s + 1 < sizeof sizebuf) sizebuf[s++] = src[in++];
        if (in + 1 >= src_len || src[in] != '\r' || src[in+1] != '\n') return -1;
        sizebuf[s] = '\0'; in += 2;
        unsigned long chunk = strtoul(sizebuf, NULL, 16);
        if (chunk == 0) { if (out >= dst_cap) return -1; dst[out] = '\0'; return (int)out; }
        if (chunk > src_len - in || chunk + out >= dst_cap) return -1;
        memcpy(dst + out, src + in, chunk); out += chunk; in += chunk;
        if (in + 1 >= src_len || src[in] != '\r' || src[in+1] != '\n') return -1;
        in += 2;
    }
    return -1;
}

static int shadowmount_post_json(const char *path, const char *body,
                                 char *json_out, size_t json_cap) {
    int fd = -1, rc = -1;
    struct sockaddr_in addr;
    struct timeval timeout = {3, 0};
    char *response = NULL, *request = NULL;
    size_t used = 0, header_end = 0;
    size_t body_len;

    if (!path || !body || !json_out || json_cap == 0) return -1;
    json_out[0] = '\0'; body_len = strlen(body);
    size_t request_cap = strlen(path) + body_len + 256;
    request = (char *)malloc(request_cap);
    if (!request) return -12;
    int request_len = snprintf(request, request_cap,
        "POST %s HTTP/1.1\r\n"
        "Host: 127.0.0.1:10101\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "Content-Length: %lu\r\n\r\n%s",
        path, (unsigned long)body_len, body);
    if (request_len < 0 || (size_t)request_len >= request_cap) { rc = -1; goto done; }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { rc = -errno; goto done; }
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof timeout);
    memset(&addr, 0, sizeof addr); addr.sin_family = AF_INET;
    addr.sin_port = htons(SHADOWMOUNT_PORT); addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) { rc = -errno; goto done; }
    if (send_all(fd, request, (size_t)request_len) != 0) { rc = -errno; goto done; }

    response = (char *)malloc(HTTP_RESPONSE_MAX + 1);
    if (!response) { rc = -12; goto done; }
    while (used < HTTP_RESPONSE_MAX) {
        ssize_t n = recv(fd, response + used, HTTP_RESPONSE_MAX - used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { rc = -errno; goto done; }
        if (n == 0) break;
        used += (size_t)n;
    }
    response[used] = '\0';
    if (find_header_end(response, used, &header_end) != 0) { rc = -1001; goto done; }
    if (strncmp(response, "HTTP/1.", 7) != 0 || !strstr(response, " 200 ")) { rc = -1002; goto done; }
    if (strstr(response, "Transfer-Encoding: chunked") || strstr(response, "transfer-encoding: chunked")) {
        rc = decode_chunked(response + header_end, used - header_end, json_out, json_cap);
        if (rc >= 0) rc = 0;
        goto done;
    }
    if (used - header_end >= json_cap) { rc = -1003; goto done; }
    memcpy(json_out, response + header_end, used - header_end);
    json_out[used - header_end] = '\0'; rc = 0;

done:
    if (response) free(response);
    if (request) free(request);
    if (fd >= 0) close(fd);
    return rc;
}

int shadowmount_get_kernel_log(char *json_out, size_t json_cap) {
    return shadowmount_post_json("/api/v1/kernel-log", "{\"max_bytes\":262144}", json_out, json_cap);
}

int shadowmount_get_games(char *json_out, size_t json_cap) {
    return shadowmount_post_json("/api/v1/games", "{\"include_size\":false}", json_out, json_cap);
}
