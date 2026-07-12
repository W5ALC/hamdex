#ifndef HAMDEX_ZIP_UTIL_H
#define HAMDEX_ZIP_UTIL_H

#include <stddef.h>

/* Reads the FCC ULS zip in memory at `data`/`size` and extracts the entry
 * whose basename (case-insensitively) is "<name>.dat" (e.g. name="en").
 * Returns a newly g_malloc'd, NUL-terminated string with the file's text
 * (decoded as UTF-8, falling back to Latin-1 byte-preservation on invalid
 * UTF-8), or NULL if the entry wasn't found / on error. Caller g_free()s. */
char *zip_read_dat(const unsigned char *data, size_t size, const char *name);

#endif
