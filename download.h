#ifndef HAMDEX_DOWNLOAD_H
#define HAMDEX_DOWNLOAD_H

#include <stddef.h>
#include <stdbool.h>

typedef void (*download_progress_cb)(const char *msg, int pct, void *user_data);

/* Downloads `url` fully into memory. On success returns a g_malloc'd buffer
 * via out_data/out_size (caller g_free()s) and returns true. On failure
 * returns false and sets *err_msg (g_malloc'd, caller g_free()s, may be NULL
 * if cancelled). `cancel_flag`, if non-NULL, is polled periodically; set it
 * to nonzero from another thread to abort the transfer. */
bool download_to_memory(const char *url,
                         unsigned char **out_data, size_t *out_size,
                         download_progress_cb progress_cb, void *user_data,
                         volatile int *cancel_flag,
                         char **err_msg);

#endif
