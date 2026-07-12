/* app.h — shared constants and small helpers for HamDex */
#ifndef HAMDEX_APP_H
#define HAMDEX_APP_H

#define HAMDEX_VERSION "1.0.0 (C rewrite of HamDex v5.1)"

/* Small helper: dup a string, treating NULL as "" */
static inline char *hx_strdup0(const char *s) {
    return g_strdup(s ? s : "");
}

#endif
