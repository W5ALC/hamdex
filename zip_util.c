#include "zip_util.h"
#include "miniz.h"
#include <glib.h>
#include <string.h>
#include <strings.h>

static const char *basename_ci(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

char *zip_read_dat(const unsigned char *data, size_t size, const char *name) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_mem(&zip, data, size, 0)) {
        return NULL;
    }
    char target[64];
    g_snprintf(target, sizeof(target), "%s.dat", name);

    int n = (int)mz_zip_reader_get_num_files(&zip);
    char *result = NULL;
    for (int i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;
        const char *bn = basename_ci(st.m_filename);
        if (strcasecmp(bn, target) != 0) continue;

        size_t out_size = 0;
        void *buf = mz_zip_reader_extract_to_heap(&zip, i, &out_size, 0);
        if (!buf) break;

        if (g_utf8_validate((const char *)buf, out_size, NULL)) {
            result = g_strndup((const char *)buf, out_size);
        } else {
            GError *err = NULL;
            gsize bytes_read = 0, bytes_written = 0;
            char *conv = g_convert((const char *)buf, (gssize)out_size,
                                    "UTF-8", "ISO-8859-1",
                                    &bytes_read, &bytes_written, &err);
            if (conv) {
                result = conv;
            } else {
                if (err) g_error_free(err);
                result = g_strndup((const char *)buf, out_size); /* best effort */
            }
        }
        mz_free(buf);
        break;
    }
    mz_zip_reader_end(&zip);
    return result;
}
