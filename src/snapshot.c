#include "snapshot.h"
#include "blob.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *sha256_hex(const uint8_t *digest) {
    static char hex[65];
    for (int i = 0; i < 32; i++)
        snprintf(hex + i*2, 3, "%02x", digest[i]);
    return hex;
}

static void sha256_bin(const char *hex, uint8_t *out) {
    for (int i = 0; i < 32; i++) {
        unsigned int b;
        sscanf(hex + i*2, "%02x", &b);
        out[i] = (uint8_t)b;
    }
}

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len > 0 ? (size_t)len + 1 : 1);
    if (!buf) { fclose(f); return NULL; }
    if (len > 0) fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    if (out_len) *out_len = (size_t)len;
    fclose(f);
    return buf;
}

// JSON escaping for strings
static char *json_escape(const char *s) {
    if (!s) return strdup("");
    char *out = malloc(strlen(s) * 2 + 3);
    char *p = out;
    *p++ = '"';
    for (const char *c = s; *c; c++) {
        if (*c == '"' || *c == '\\') { *p++ = '\\'; *p++ = *c; }
        else if (*c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (*c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else if (*c == '\t') { *p++ = '\\'; *p++ = 't'; }
        else *p++ = *c;
    }
    *p++ = '"';
    *p = '\0';
    return out;
}

static void json_write_str(FILE *f, const char *key, const char *val) {
    char *jval = json_escape(val);
    fprintf(f, "\"%s\":%s", key, jval);
    free(jval);
}

Snapshot *snapshot_create(CAS *cas, const char *tree_json,
                          const char *message, const char *author,
                          const char *parent_sha) {
    // Build snapshot JSON
    char *snap_json = NULL;
    {
        FILE *f = open_memstream(&snap_json, &(size_t){0});
        fprintf(f, "{");
        json_write_str(f, "tree", tree_json);
        fprintf(f, ",");
        json_write_str(f, "message", message);
        fprintf(f, ",");
        json_write_str(f, "author", author);
        fprintf(f, ",");
        fprintf(f, "\"timestamp\":%ld", (long)time(NULL));
        fprintf(f, ",");
        if (parent_sha) {
            fprintf(f, "\"parent\":");
            json_escape(parent_sha);
            fprintf(f, "\"%s\"", parent_sha);
        } else {
            fprintf(f, "\"parent\":null");
        }
        fprintf(f, "}");
        fclose(f);
    }
    size_t snap_len;
    uint8_t *snap_data = (uint8_t *)snap_json;
    size_t blob_len;
    uint8_t *blob = blob_build(snap_data, snap_len, &blob_len);
    free(snap_json);
    if (!blob) return NULL;

    uint8_t digest[32];
    sha256_hash(blob, blob_len, digest);
    char sha_hex[65];
    sha256_hex_to(sha_hex, digest);

    bool ok = cas_write(cas, sha_hex, blob, blob_len);
    free(blob);
    if (!ok) return NULL;

    Snapshot *s = calloc(1, sizeof(Snapshot));
    strncpy(s->sha, sha_hex, 64);
    s->sha[64] = '\0';
    s->tree_json = strdup(tree_json ? tree_json : "{}");
    s->message = strdup(message ? message : "");
    s->author = strdup(author ? author : "anonymous");
    s->timestamp = time(NULL);
    s->parent_sha = parent_sha ? strdup(parent_sha) : NULL;
    return s;
}

static Snapshot *parse_snapshot_json(const char *sha_hex, const uint8_t *blob, size_t blob_len) {
    size_t data_len;
    uint8_t *data = blob_read(blob, blob_len, &data_len);
    if (!data) return NULL;

    Snapshot *s = calloc(1, sizeof(Snapshot));
    strncpy(s->sha, sha_hex, 64);
    s->sha[64] = '\0';
    s->tree_json = NULL;
    s->message = s->author = NULL;
    s->timestamp = 0;
    s->parent_sha = NULL;

    // Parse key "value" pairs manually (no external JSON lib)
    char *copy = strndup((char *)data, data_len);

    #define FIND_KEY(k) strstr(copy, "\"" k "\"")
    #define READ_STR_VAL(k) do { \
        char *kv = FIND_KEY(k); \
        if (kv) { \
            char *colon = strchr(kv, ':'); \
            if (colon) { \
                char *p = colon + 1; while (*p && (isspace((unsigned char)*p))) p++; \
                if (*p == '"') { \
                    p++; char *end = p; while (*end && *end != '"') end++; \
                    size_t len = (size_t)(end - p); \
                    if (strcmp(k, "tree") == 0) { s->tree_json = strndup(p, len); } \
                    else if (strcmp(k, "message") == 0) { s->message = strndup(p, len); } \
                    else if (strcmp(k, "author") == 0) { s->author = strndup(p, len); } \
                } else if (*p == 'n' && strncmp(p, "null", 4) == 0) { /* null */ } \
                else if (isdigit((unsigned char)*p)) { \
                    s->timestamp = (time_t)strtod(p, NULL); \
                } \
            } \
        } \
    } while(0)

    READ_STR_VAL("tree");
    READ_STR_VAL("message");
    READ_STR_VAL("author");

    // Parse parent
    char *pv = strstr(copy, "\"parent\"");
    if (pv) {
        char *c2 = strchr(pv, ':'); if (c2) { c2++; while (*c2 && isspace((unsigned char)*c2)) c2++;
            if (*c2 == '"') {
                c2++; char *e2 = c2; while (*e2 && *e2 != '"') e2++;
                s->parent_sha = strndup(c2, (size_t)(e2 - c2));
            }
        }
    }

    #undef FIND_KEY
    #undef READ_STR_VAL
    free(copy);
    free(data);
    return s;
}

Snapshot *snapshot_get(CAS *cas, const char *sha_hex) {
    uint8_t *data = NULL;
    size_t len = 0;
    if (!cas_read(cas, sha_hex, &data, &len)) return NULL;
    Snapshot *s = parse_snapshot_json(sha_hex, data, len);
    free(data);
    return s;
}

void snapshot_free(Snapshot *s) {
    if (!s) return;
    free(s->tree_json);
    free(s->message);
    free(s->author);
    free(s->parent_sha);
    free(s);
}

static char *find_json_str(char *obj, const char *key) {
    char *kv = strstr(obj, key);
    if (!kv) return NULL;
    char *colon = strchr(kv, ':');
    if (!colon) return NULL;
    char *p = colon + 1;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return NULL;
    p++;
    char *end = p;
    while (*end && *end != '"') end++;
    return strndup(p, (size_t)(end - p));
}

static char *find_json_raw_str(char *obj, const char *key) {
    char *kv = strstr(obj, key);
    if (!kv) return NULL;
    char *colon = strchr(kv, ':');
    if (!colon) return NULL;
    char *p = colon + 1;
    while (*p && (isspace((unsigned char)*p))) p++;
    if (*p != '"') return NULL;
    p++;
    char *end = p;
    while (*end && *end != '"') end++;
    return strndup(p, (size_t)(end - p));
}

Tree tree_parse(const char *tree_json) {
    Tree t = {0};
    if (!tree_json) return t;
    char *copy = strdup(tree_json);
    char *p = copy;
    while (*p) {
        while (*p && *p != '"') p++;
        if (!*p) break;
        p++; // skip opening quote
        char *key_end = p;
        while (*key_end && *key_end != '"') key_end++;
        char *key = strndup(p, (size_t)(key_end - p));
        p = key_end + 1; // skip closing quote
        while (*p && *p != ':') p++;
        p++; // skip colon
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '"') {
            p++; char *val_end = p;
            while (*val_end && *val_end != '"') val_end++;
            char *val = strndup(p, (size_t)(val_end - p));
            t.paths = realloc(t.paths, (t.count + 1) * sizeof(char *));
            t.shas = realloc(t.shas, (t.count + 1) * sizeof(char *));
            t.paths[t.count] = key;
            t.shas[t.count] = val;
            t.count++;
            p = val_end + 1;
        } else {
            free(key);
            break;
        }
    }
    free(copy);
    return t;
}

void tree_free(Tree t) {
    for (int i = 0; i < t.count; i++) { free(t.paths[i]); free(t.shas[i]); }
    free(t.paths); free(t.shas);
}

static char *json_str_escape(const char *s) {
    if (!s) return strdup("\"\"");
    char *out = malloc(strlen(s) * 2 + 3);
    char *p = out;
    *p++ = '"';
    for (const char *c = s; *c; c++) {
        if (*c == '"' || *c == '\\') { *p++ = '\\'; *p++ = *c; }
        else if (*c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else *p++ = *c;
    }
    *p++ = '"'; *p = '\0';
    return out;
}

char *tree_build_json(char **paths, char **shas, int count) {
    char *result = NULL;
    FILE *f = open_memstream(&result, &(size_t){0});
    fputc('{', f);
    for (int i = 0; i < count; i++) {
        if (i > 0) fputc(',', f);
        char *jpath = json_str_escape(paths[i]);
        char *jsha = json_str_escape(shas[i]);
        fprintf(f, "%s:%s", jpath, jsha);
        free(jpath); free(jsha);
    }
    fputc('}', f);
    fclose(f);
    return result;
}

Change **snapshot_diff(CAS *cas, const char *sha1, const char *sha2, int *out_count) {
    *out_count = 0;
    Change **changes = NULL;

    Snapshot *s1 = snapshot_get(cas, sha1);
    Snapshot *s2 = snapshot_get(cas, sha2);
    if (!s1 || !s2) { if(s1)snapshot_free(s1); if(s2)snapshot_free(s2); return NULL; }

    Tree t1 = tree_parse(s1->tree_json);
    Tree t2 = tree_parse(s2->tree_json);

    // Build lookup maps
    char seen[256] = {0};
    for (int i = 0; i < t1.count; i++) {
        seen[(unsigned char)((i < 128 ? i : 0))] = 1; // just track count
    }
    (void)seen;

    for (int i = 0; i < t1.count; i++) {
        char *old_sha = NULL;
        char *new_sha = NULL;
        char action = 0;
        for (int j = 0; j < t2.count; j++) {
            if (strcmp(t1.paths[i], t2.paths[j]) == 0) {
                if (strcmp(t1.shas[i], t2.shas[j]) == 0) {
                    break; // unchanged
                } else {
                    old_sha = t1.shas[i];
                    new_sha = t2.shas[j];
                    action = 'M';
                    break;
                }
            }
        }
        if (action == 0) {
            // Not found in t2 = deleted
            old_sha = t1.shas[i];
            new_sha = NULL;
            action = 'D';
        }
        if (action) {
            changes = realloc(changes, (*out_count + 1) * sizeof(Change *));
            Change *c = malloc(sizeof(Change));
            c->action = action;
            c->path = strdup(t1.paths[i]);
            c->old_sha = old_sha ? strdup(old_sha) : NULL;
            c->new_sha = new_sha ? strdup(new_sha) : NULL;
            changes[*out_count] = c;
            (*out_count)++;
        }
    }
    for (int j = 0; j < t2.count; j++) {
        bool found = false;
        for (int i = 0; i < t1.count; i++) {
            if (strcmp(t2.paths[j], t1.paths[i]) == 0) { found = true; break; }
        }
        if (!found) {
            changes = realloc(changes, (*out_count + 1) * sizeof(Change *));
            Change *c = malloc(sizeof(Change));
            c->action = 'A';
            c->path = strdup(t2.paths[j]);
            c->old_sha = NULL;
            c->new_sha = strdup(t2.shas[j]);
            changes[*out_count] = c;
            (*out_count)++;
        }
    }

    tree_free(t1);
    tree_free(t2);
    snapshot_free(s1);
    snapshot_free(s2);
    return changes;
}

void changes_free(Change **changes, int count) {
    if (!changes) return;
    for (int i = 0; i < count; i++) {
        free(changes[i]->path);
        free(changes[i]->old_sha);
        free(changes[i]->new_sha);
        free(changes[i]);
    }
    free(changes);
}
