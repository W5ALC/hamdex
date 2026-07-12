#include "fcc_parse.h"
#include <string.h>

/* Splits `line` on '|' and returns a GPtrArray of newly-allocated strings
 * (already stripped of surrounding whitespace). Caller frees with
 * g_ptr_array_free(arr, TRUE) (frees elements too, since we set a free func). */
static GPtrArray *split_pipe(const char *line) {
    GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);
    const char *p = line;
    while (1) {
        const char *bar = strchr(p, '|');
        size_t len = bar ? (size_t)(bar - p) : strlen(p);
        char *field = g_strndup(p, len);
        char *trimmed = g_strstrip(field);
        g_ptr_array_add(arr, g_strdup(trimmed));
        g_free(field);
        if (!bar) break;
        p = bar + 1;
    }
    return arr;
}

static const char *field_at(GPtrArray *arr, guint idx) {
    if (idx >= arr->len) return "";
    const char *v = g_ptr_array_index(arr, idx);
    return v ? v : "";
}

/* Column indices, per the layout documented in fcc_parse.h */
enum { EN_UID = 1, EN_CALL = 4, EN_ENTITY_TYPE = 5, EN_ENTITY_NAME = 7,
       EN_FIRST = 8, EN_LAST = 10, EN_ADDR = 15, EN_CITY = 16, EN_STATE = 17,
       EN_ZIP = 18, EN_FRN = 22, EN_APPTYPE = 23 };
enum { HD_UID = 1, HD_CALL = 4, HD_STATUS = 5, HD_SERVICE = 6, HD_GRANT = 7,
       HD_EXPIRED = 8, HD_CANCEL = 9, HD_EFFECTIVE = 42 };
enum { AM_UID = 1, AM_CALL = 4, AM_CLASS = 5, AM_GROUP = 6, AM_TRUSTEE_CALL = 8,
       AM_PREV_CALL = 15, AM_TRUSTEE_NAME = 17 };

static void free_lines(char **lines) { g_strfreev(lines); }

/* Row destructors. These are registered directly as each hash table's value
 * destructor (see g_hash_table_new_full calls below) so there is exactly one
 * place responsible for freeing a row's memory. Previously fcc_en/hd/am_
 * table_free() manually freed each row's fields *and* g_hash_table_destroy()
 * was separately registered with a plain g_free as the value destructor —
 * meaning every row got freed twice (once by the manual loop, once more by
 * g_hash_table_destroy() calling g_free() on the same already-freed
 * pointer), corrupting the heap. */
static void en_row_free(gpointer p) {
    FccEnRow *r = p;
    g_free(r->call_sign); g_free(r->entity_type); g_free(r->entity_name);
    g_free(r->first_name); g_free(r->last_name); g_free(r->street_address);
    g_free(r->city); g_free(r->state); g_free(r->zip_code); g_free(r->frn);
    g_free(r->applicant_type_code); g_free(r);
}
static void hd_row_free(gpointer p) {
    FccHdRow *r = p;
    g_free(r->call_sign); g_free(r->license_status); g_free(r->radio_service_code);
    g_free(r->grant_date); g_free(r->expired_date); g_free(r->cancellation_date);
    g_free(r->effective_date); g_free(r);
}
static void am_row_free(gpointer p) {
    FccAmRow *r = p;
    g_free(r->call_sign); g_free(r->operator_class); g_free(r->group_code);
    g_free(r->trustee_call_sign); g_free(r->trustee_name); g_free(r->previous_call_sign);
    g_free(r);
}

GHashTable *fcc_parse_en(const char *text) {
    GHashTable *t = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, en_row_free);
    if (!text || !*text) return t;
    char **lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp; lp++) {
        if (!**lp) continue;
        GPtrArray *f = split_pipe(*lp);
        const char *uid = field_at(f, EN_UID);
        if (uid && *uid) {
            FccEnRow *row = g_new0(FccEnRow, 1);
            row->call_sign            = g_strdup(field_at(f, EN_CALL));
            row->entity_type          = g_strdup(field_at(f, EN_ENTITY_TYPE));
            row->entity_name          = g_strdup(field_at(f, EN_ENTITY_NAME));
            row->first_name           = g_strdup(field_at(f, EN_FIRST));
            row->last_name            = g_strdup(field_at(f, EN_LAST));
            row->street_address       = g_strdup(field_at(f, EN_ADDR));
            row->city                 = g_strdup(field_at(f, EN_CITY));
            row->state                = g_strdup(field_at(f, EN_STATE));
            row->zip_code             = g_strdup(field_at(f, EN_ZIP));
            row->frn                  = g_strdup(field_at(f, EN_FRN));
            row->applicant_type_code  = g_strdup(field_at(f, EN_APPTYPE));
            g_hash_table_insert(t, g_strdup(uid), row);
        }
        g_ptr_array_free(f, TRUE);
    }
    free_lines(lines);
    return t;
}

GHashTable *fcc_parse_hd(const char *text) {
    GHashTable *t = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, hd_row_free);
    if (!text || !*text) return t;
    char **lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp; lp++) {
        if (!**lp) continue;
        GPtrArray *f = split_pipe(*lp);
        const char *uid = field_at(f, HD_UID);
        if (uid && *uid) {
            FccHdRow *row = g_new0(FccHdRow, 1);
            row->call_sign         = g_strdup(field_at(f, HD_CALL));
            row->license_status    = g_strdup(field_at(f, HD_STATUS));
            row->radio_service_code= g_strdup(field_at(f, HD_SERVICE));
            row->grant_date        = g_strdup(field_at(f, HD_GRANT));
            row->expired_date      = g_strdup(field_at(f, HD_EXPIRED));
            row->cancellation_date = g_strdup(field_at(f, HD_CANCEL));
            row->effective_date    = g_strdup(field_at(f, HD_EFFECTIVE));
            g_hash_table_insert(t, g_strdup(uid), row);
        }
        g_ptr_array_free(f, TRUE);
    }
    free_lines(lines);
    return t;
}

GHashTable *fcc_parse_am(const char *text) {
    GHashTable *t = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, am_row_free);
    if (!text || !*text) return t;
    char **lines = g_strsplit(text, "\n", -1);
    for (char **lp = lines; *lp; lp++) {
        if (!**lp) continue;
        GPtrArray *f = split_pipe(*lp);
        const char *uid = field_at(f, AM_UID);
        if (uid && *uid) {
            FccAmRow *row = g_new0(FccAmRow, 1);
            row->call_sign          = g_strdup(field_at(f, AM_CALL));
            row->operator_class     = g_strdup(field_at(f, AM_CLASS));
            row->group_code         = g_strdup(field_at(f, AM_GROUP));
            row->trustee_call_sign  = g_strdup(field_at(f, AM_TRUSTEE_CALL));
            row->trustee_name       = g_strdup(field_at(f, AM_TRUSTEE_NAME));
            row->previous_call_sign = g_strdup(field_at(f, AM_PREV_CALL));
            g_hash_table_insert(t, g_strdup(uid), row);
        }
        g_ptr_array_free(f, TRUE);
    }
    free_lines(lines);
    return t;
}

void fcc_en_table_free(GHashTable *t) {
    if (t) g_hash_table_destroy(t);
}
void fcc_hd_table_free(GHashTable *t) {
    if (t) g_hash_table_destroy(t);
}
void fcc_am_table_free(GHashTable *t) {
    if (t) g_hash_table_destroy(t);
}
