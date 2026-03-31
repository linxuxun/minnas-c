#include "blob.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

uint8_t *blob_build(const uint8_t *data, size_t data_len, size_t *out_len) {
    if (!data || !out_len) return NULL;
    char header[32];
    int hlen = snprintf(header, sizeof(header), "blob %zu\0", data_len);
    // header includes the null byte between size and data
    size_t blob_len = (size_t)hlen + data_len;
    uint8_t *blob = malloc(blob_len);
    if (!blob) return NULL;
    memcpy(blob, header, (size_t)hlen);
    memcpy(blob + hlen, data, data_len);
    *out_len = blob_len;
    return blob;
}

static size_t parse_size(const char *s) {
    while (*s && !isdigit((unsigned char)*s)) s++;
    if (!*s) return 0;
    size_t n = 0;
    while (isdigit((unsigned char)*s)) {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

uint8_t *blob_read(const uint8_t *blob, size_t blob_len, size_t *out_data_len) {
    if (!blob || blob_len == 0 || !out_data_len) return NULL;
    if (memcmp(blob, "blob ", 5) != 0) return NULL;
    size_t size = parse_size((const char *)blob + 5);
    // header ends at \0 after size field
    const uint8_t *p = blob + 5;
    while (*p != '\0' && (size_t)(p - blob) < blob_len) p++;
    p++; // skip the \0
    if ((size_t)(p - blob) > blob_len) return NULL;
    size_t data_start = (size_t)(p - blob);
    if (data_start + size != blob_len) return NULL;
    uint8_t *copy = malloc(size);
    if (!copy) return NULL;
    memcpy(copy, blob + data_start, size);
    *out_data_len = size;
    return copy;
}

bool blob_verify(const uint8_t *blob, size_t blob_len) {
    if (!blob || blob_len < 6) return false;
    if (memcmp(blob, "blob ", 5) != 0) return false;
    size_t size = parse_size((const char *)blob + 5);
    const uint8_t *p = blob + 5;
    while (*p != '\0' && (size_t)(p - blob) < blob_len) p++;
    p++;
    return (size_t)(p - blob) + size == blob_len;
}

const char *blob_describe(const uint8_t *blob, size_t blob_len) {
    static char buf[128];
    if (!blob_verify(blob, blob_len)) {
        snprintf(buf, sizeof(buf), "<invalid blob len=%zu>", blob_len);
        return buf;
    }
    size_t data_len;
    blob_read(blob, blob_len, &data_len);
    snprintf(buf, sizeof(buf), "blob size=%zu data=%zu bytes",
              blob_len, data_len);
    return buf;
}
