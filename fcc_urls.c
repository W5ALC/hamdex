#include "hamdex_app.h"
#include <time.h>
#include <string.h>
#include <ctype.h>

const char *hx_fcc_weekly_url(void) {
    return "https://data.fcc.gov/download/pub/uls/complete/l_amat.zip";
}

const char *hx_fcc_daily_url(void) {
    static char buf[128];
    time_t now = time(NULL);
    now -= 24 * 60 * 60; /* yesterday */
    struct tm tmv;
    localtime_r(&now, &tmv);
    char wday[8];
    strftime(wday, sizeof(wday), "%a", &tmv);
    for (char *c = wday; *c; c++) *c = (char)tolower((unsigned char)*c);
    g_snprintf(buf, sizeof(buf), "https://data.fcc.gov/download/pub/uls/daily/l_am_%s.zip", wday);
    return buf;
}
