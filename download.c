#include "download.h"
#include <curl/curl.h>
#include <glib.h>
#include <string.h>

typedef struct {
    GByteArray *buf;
    download_progress_cb progress_cb;
    void *user_data;
    volatile int *cancel_flag;
    curl_off_t total;
} DlCtx;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    DlCtx *ctx = userdata;
    g_byte_array_append(ctx->buf, ptr, (guint)(size * nmemb));
    if (ctx->progress_cb) {
        char msg[128];
        if (ctx->total > 0) {
            int pct = (int)((double)ctx->buf->len / (double)ctx->total * 18.0); /* 0-18%, rest is import */
            g_snprintf(msg, sizeof(msg), "Downloading... %u/%lld MB",
                       ctx->buf->len / 1048576, (long long)(ctx->total / 1048576));
            ctx->progress_cb(msg, pct, ctx->user_data);
        } else {
            g_snprintf(msg, sizeof(msg), "Downloading... %u MB", ctx->buf->len / 1048576);
            ctx->progress_cb(msg, 0, ctx->user_data);
        }
    }
    return size * nmemb;
}

static int xferinfo_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                        curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; (void)dlnow; (void)ultotal; (void)ulnow;
    DlCtx *ctx = clientp;
    if (ctx->cancel_flag && *ctx->cancel_flag) return 1; /* abort */
    return 0;
}

/* One attempt at the download. Returns the CURLcode and (on a completed
 * HTTP response) the status code, and leaves whatever bytes were received
 * in ctx->buf (caller resets/frees it between retries). */
static CURLcode try_download(const char *url, DlCtx *ctx, long *http_code) {
    CURL *curl = curl_easy_init();
    if (!curl) return CURLE_FAILED_INIT;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HamDex/C-1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    /* data.fcc.gov is documented to be intermittently slow to respond
     * (community reports of it hanging for extended periods, unrelated to
     * client config) so this is generous rather than tight. */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    /* No fixed total-transfer timeout: the weekly ULS file is ~100MB+, which
     * can easily take a while even when the connection is healthy. Abort
     * only if the transfer actually stalls (under 1 byte/sec for 90s
     * straight), not just because it's taking a while. */
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 90L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferinfo_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, ctx);

    CURLcode res = curl_easy_perform(curl);
    *http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    curl_easy_cleanup(curl);
    return res;
}

#define MAX_ATTEMPTS 4

bool download_to_memory(const char *url,
                         unsigned char **out_data, size_t *out_size,
                         download_progress_cb progress_cb, void *user_data,
                         volatile int *cancel_flag,
                         char **err_msg) {
    *out_data = NULL;
    *out_size = 0;
    if (err_msg) *err_msg = NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    bool ok = false;
    CURLcode res = CURLE_OK;
    long http_code = 0;
    DlCtx ctx = {0};

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        if (cancel_flag && *cancel_flag) break;

        if (ctx.buf) g_byte_array_free(ctx.buf, TRUE);
        ctx.buf = g_byte_array_new();
        ctx.progress_cb = progress_cb;
        ctx.user_data = user_data;
        ctx.cancel_flag = cancel_flag;
        ctx.total = 0;

        if (progress_cb) {
            char msg[96];
            if (attempt == 1) g_strlcpy(msg, "Connecting to FCC...", sizeof(msg));
            else g_snprintf(msg, sizeof(msg), "FCC server is slow to respond, retrying (%d/%d)...",
                             attempt, MAX_ATTEMPTS);
            progress_cb(msg, 0, user_data);
        }

        res = try_download(url, &ctx, &http_code);

        if (res == CURLE_ABORTED_BY_CALLBACK) {
            break; /* user cancelled; don't retry */
        }
        if (res == CURLE_OK && http_code < 400) {
            ok = true;
            break;
        }
        /* Don't retry on a definitive client error (bad URL, not found,
         * etc.) — only on things that plausibly resolve themselves, like
         * timeouts, connection resets, or a transient 5xx from the server. */
        gboolean retryable = (res == CURLE_OPERATION_TIMEDOUT ||
                               res == CURLE_COULDNT_CONNECT ||
                               res == CURLE_RECV_ERROR ||
                               res == CURLE_SEND_ERROR ||
                               res == CURLE_GOT_NOTHING ||
                               (res == CURLE_OK && http_code >= 500));
        if (!retryable || attempt == MAX_ATTEMPTS) break;

        /* Brief backoff before retrying, but stay responsive to cancellation. */
        for (int waited = 0; waited < 5; waited++) {
            if (cancel_flag && *cancel_flag) break;
            g_usleep(1000000);
        }
    }

    if (!ok) {
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            if (err_msg) *err_msg = NULL; /* cancelled, not an error */
        } else if (res != CURLE_OK) {
            if (err_msg) *err_msg = g_strdup_printf(
                "%s (the FCC download server is known to be intermittently slow — try again in a few minutes)",
                curl_easy_strerror(res));
        } else if (http_code >= 400) {
            if (err_msg) *err_msg = g_strdup_printf("Server returned HTTP %ld", http_code);
        }
    }

    curl_global_cleanup();

    if (ok) {
        *out_size = ctx.buf->len;
        *out_data = g_byte_array_free(ctx.buf, FALSE);
    } else if (ctx.buf) {
        g_byte_array_free(ctx.buf, TRUE);
    }
    return ok;
}
