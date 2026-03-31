/*
 * utils.h - Utility functions (no external dependencies)
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>

/* String utilities */
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* Join path components (caller frees) */
char *path_join(const char *a, const char *b);

/* Normalize a path (remove . and ..) (caller frees) */
char *path_normalize(const char *path);

/* Get directory name of a path (caller frees) */
char *path_dirname(const char *path);

/* Get basename of a path (caller frees) */
char *path_basename(const char *path);

/* Ensure directory exists */
int ensure_dir(const char *path);

/* Remove directory recursively */
int remove_dir_recursive(const char *path);

/* Check if file exists */
bool file_exists(const char *path);

/* Read entire file (caller frees) */
int read_file(const char *path, char **out_data, size_t *out_len);
int read_binary_file(const char *path, uint8_t **out_data, size_t *out_len);

/* Write file */
int write_file(const char *path, const char *data, size_t len);
int write_binary_file(const char *path, const uint8_t *data, size_t len);

/* Append to file */
int append_file(const char *path, const char *data, size_t len);

/* Get file size */
long file_size(const char *path);

/* Copy file */
int copy_file(const char *src, const char *dst);

/* String list utilities */
char **strlist_append(char **list, int *count, const char *s);
void strlist_free(char **list, int count);

/* Parse hex string to binary (caller frees, or NULL on error) */
uint8_t *hex_to_bin(const char *hex, size_t *out_len);

/* Convert binary to hex string (caller frees) */
char *bin_to_hex(const uint8_t *bin, size_t len);

/* Trim whitespace */
char *strtrim(char *s);

/* Skip leading whitespace */
const char *skip_ws(const char *s);

/* Check if string ends with suffix */
bool str_ends_with(const char *s, const char *suffix);

/* Min/max macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* UTILS_H */
