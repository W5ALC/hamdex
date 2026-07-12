#include "fcc_db.h"
#include "fcc_parse.h"
#include "zip_util.h"
#include <string.h>
#include <stdio.h>

static char *default_fcc_db_path(void) {
    const char *home = g_get_home_dir();
    char *dir = g_build_filename(home, ".config", "hamdex", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *path = g_build_filename(dir, "fcc_uls.db", NULL);
    g_free(dir);
    return path;
}

static char *now_iso8601(void) {
    GDateTime *dt = g_date_time_new_now_local();
    char *s = g_date_time_format(dt, "%Y-%m-%dT%H:%M:%S");
    g_date_time_unref(dt);
    return s;
}

FccDb *fccdb_open(const char *path) {
    FccDb *db = g_new0(FccDb, 1);
    char *p = path ? g_strdup(path) : default_fcc_db_path();
    if (sqlite3_open(p, &db->conn) != SQLITE_OK) {
        g_printerr("fccdb: failed to open %s: %s\n", p, sqlite3_errmsg(db->conn));
    }
    const char *schema =
        "CREATE TABLE IF NOT EXISTS fcc_licenses ("
        "  call_sign TEXT, entity_name TEXT, first_name TEXT, last_name TEXT,"
        "  street_address TEXT, city TEXT, state TEXT, zip_code TEXT,"
        "  license_status TEXT, grant_date TEXT, expired_date TEXT,"
        "  effective_date TEXT, operator_class TEXT, radio_service_code TEXT,"
        "  unique_id TEXT PRIMARY KEY,"
        "  entity_type TEXT, applicant_type_code TEXT,"
        "  trustee_call_sign TEXT, trustee_name TEXT,"
        "  previous_call_sign TEXT, group_code TEXT,"
        "  frn TEXT, cancellation_date TEXT"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fcc_call ON fcc_licenses(call_sign);"
        "CREATE INDEX IF NOT EXISTS idx_fcc_name ON fcc_licenses(entity_name);"
        "CREATE INDEX IF NOT EXISTS idx_fcc_city ON fcc_licenses(city);"
        "CREATE INDEX IF NOT EXISTS idx_fcc_state ON fcc_licenses(state);"
        "CREATE INDEX IF NOT EXISTS idx_fcc_address ON fcc_licenses(street_address);"
        "CREATE TABLE IF NOT EXISTS fcc_meta (key TEXT PRIMARY KEY, value TEXT);";
    char *errmsg = NULL;
    if (sqlite3_exec(db->conn, schema, NULL, NULL, &errmsg) != SQLITE_OK) {
        g_printerr("fccdb: schema init failed: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    g_free(p);
    return db;
}

void fccdb_close(FccDb *db) {
    if (!db) return;
    if (db->conn) sqlite3_close(db->conn);
    g_free(db);
}

void fcc_record_free(FccRecord *r) {
    if (!r) return;
    g_free(r->call_sign); g_free(r->entity_name); g_free(r->first_name); g_free(r->last_name);
    g_free(r->street_address); g_free(r->city); g_free(r->state); g_free(r->zip_code);
    g_free(r->license_status); g_free(r->operator_class); g_free(r->grant_date); g_free(r->expired_date);
    g_free(r->effective_date); g_free(r->cancellation_date);
    g_free(r->entity_type); g_free(r->applicant_type_code);
    g_free(r->group_code); g_free(r->trustee_call_sign); g_free(r->trustee_name); g_free(r->previous_call_sign);
    g_free(r->frn); g_free(r->unique_id);
    g_free(r);
}

/* ---- import ---- */

static void report(download_progress_cb cb, void *ud, const char *msg, int pct) {
    if (cb) cb(msg, pct, ud);
}

long fccdb_import_zip_bytes(FccDb *db, const unsigned char *data, size_t size,
                             gboolean is_daily,
                             download_progress_cb progress_cb, void *user_data,
                             char **err_msg) {
    if (err_msg) *err_msg = NULL;
    report(progress_cb, user_data, "Extracting ZIP...", 20);

    char *en_text = zip_read_dat(data, size, "en");
    char *hd_text = zip_read_dat(data, size, "hd");
    char *am_text = zip_read_dat(data, size, "am");

    report(progress_cb, user_data, "Parsing EN...", 35);
    GHashTable *en = fcc_parse_en(en_text);
    report(progress_cb, user_data, "Parsing HD...", 50);
    GHashTable *hd = fcc_parse_hd(hd_text);
    report(progress_cb, user_data, "Parsing AM...", 65);
    GHashTable *am = fcc_parse_am(am_text);
    g_free(en_text); g_free(hd_text); g_free(am_text);

    if (g_hash_table_size(en) == 0 && g_hash_table_size(hd) == 0) {
        fcc_en_table_free(en); fcc_hd_table_free(hd); fcc_am_table_free(am);
        if (err_msg) *err_msg = g_strdup("ZIP did not contain recognizable EN/HD records (not a valid ULS ZIP?).");
        return -1;
    }

    /* union of uids present in en or hd */
    GHashTable *uid_set = g_hash_table_new(g_str_hash, g_str_equal);
    GHashTableIter it; gpointer k, v;
    g_hash_table_iter_init(&it, en);
    while (g_hash_table_iter_next(&it, &k, &v)) g_hash_table_add(uid_set, k);
    g_hash_table_iter_init(&it, hd);
    while (g_hash_table_iter_next(&it, &k, &v)) g_hash_table_add(uid_set, k);

    if (!is_daily) {
        report(progress_cb, user_data, "Clearing old data...", 75);
        sqlite3_exec(db->conn, "DELETE FROM fcc_licenses", NULL, NULL, NULL);
    } else {
        report(progress_cb, user_data, "Merging daily changes...", 75);
    }

    report(progress_cb, user_data, "Writing to database...", 80);
    sqlite3_exec(db->conn, "PRAGMA journal_mode=WAL", NULL, NULL, NULL);
    sqlite3_exec(db->conn, "PRAGMA synchronous=NORMAL", NULL, NULL, NULL);
    sqlite3_exec(db->conn, "BEGIN TRANSACTION", NULL, NULL, NULL);

    sqlite3_stmt *stmt;
    const char *sql =
        "INSERT OR REPLACE INTO fcc_licenses "
        "(call_sign,entity_name,first_name,last_name,street_address,city,state,"
        " zip_code,license_status,grant_date,expired_date,effective_date,"
        " operator_class,radio_service_code,unique_id,"
        " entity_type,applicant_type_code,trustee_call_sign,trustee_name,"
        " previous_call_sign,group_code,frn,cancellation_date) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);

    long written = 0;
    g_hash_table_iter_init(&it, uid_set);
    while (g_hash_table_iter_next(&it, &k, &v)) {
        const char *uid = (const char *)k;
        FccEnRow *e = g_hash_table_lookup(en, uid);
        FccHdRow *h = g_hash_table_lookup(hd, uid);
        FccAmRow *a = g_hash_table_lookup(am, uid);

        const char *call = (e && e->call_sign && *e->call_sign) ? e->call_sign
                          : (h && h->call_sign) ? h->call_sign : "";
        if (!call || !*call) continue;
        char *call_up = g_ascii_strup(call, -1);

        int i = 1;
        sqlite3_bind_text(stmt, i++, call_up, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->entity_name : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->first_name : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->last_name : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->street_address : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->city : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->state : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->zip_code : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, h ? h->license_status : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, h ? h->grant_date : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, h ? h->expired_date : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, h ? h->effective_date : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, a ? a->operator_class : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, h ? h->radio_service_code : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, uid, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->entity_type : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->applicant_type_code : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, a ? a->trustee_call_sign : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, a ? a->trustee_name : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, a ? a->previous_call_sign : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, a ? a->group_code : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, e ? e->frn : "", -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, i++, h ? h->cancellation_date : "", -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) == SQLITE_DONE) written++;
        sqlite3_reset(stmt);
        g_free(call_up);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db->conn, "COMMIT", NULL, NULL, NULL);

    g_hash_table_destroy(uid_set);
    fcc_en_table_free(en);
    fcc_hd_table_free(hd);
    fcc_am_table_free(am);

    long total_count = 0;
    sqlite3_stmt *cs;
    sqlite3_prepare_v2(db->conn, "SELECT COUNT(*) FROM fcc_licenses", -1, &cs, NULL);
    if (sqlite3_step(cs) == SQLITE_ROW) total_count = sqlite3_column_int64(cs, 0);
    sqlite3_finalize(cs);

    char *ts = now_iso8601();
    sqlite3_stmt *meta;
    sqlite3_prepare_v2(db->conn, "INSERT OR REPLACE INTO fcc_meta VALUES ('last_import',?)", -1, &meta, NULL);
    sqlite3_bind_text(meta, 1, ts, -1, SQLITE_TRANSIENT);
    sqlite3_step(meta); sqlite3_finalize(meta);
    char count_str[32];
    g_snprintf(count_str, sizeof(count_str), "%ld", total_count);
    sqlite3_prepare_v2(db->conn, "INSERT OR REPLACE INTO fcc_meta VALUES ('record_count',?)", -1, &meta, NULL);
    sqlite3_bind_text(meta, 1, count_str, -1, SQLITE_TRANSIENT);
    sqlite3_step(meta); sqlite3_finalize(meta);
    g_free(ts);

    char donemsg[128];
    if (is_daily)
        g_snprintf(donemsg, sizeof(donemsg), "Done!  %ld records merged  |  %ld total", written, total_count);
    else
        g_snprintf(donemsg, sizeof(donemsg), "Done!  %ld records imported", total_count);
    report(progress_cb, user_data, donemsg, 100);

    return written;
}

long fccdb_import_local_file(FccDb *db, const char *path, gboolean is_daily,
                              download_progress_cb progress_cb, void *user_data,
                              char **err_msg) {
    if (err_msg) *err_msg = NULL;
    report(progress_cb, user_data, "Opening local ZIP...", 5);
    gchar *contents = NULL;
    gsize length = 0;
    GError *gerr = NULL;
    if (!g_file_get_contents(path, &contents, &length, &gerr)) {
        if (err_msg) *err_msg = g_strdup_printf("Could not read file: %s", gerr ? gerr->message : "unknown error");
        if (gerr) g_error_free(gerr);
        return -1;
    }
    long result = fccdb_import_zip_bytes(db, (const unsigned char *)contents, length, is_daily,
                                          progress_cb, user_data, err_msg);
    g_free(contents);
    return result;
}

long fccdb_download_and_import(FccDb *db, const char *url, gboolean is_daily,
                                download_progress_cb progress_cb, void *user_data,
                                volatile int *cancel_flag, char **err_msg) {
    if (err_msg) *err_msg = NULL;
    unsigned char *data = NULL;
    size_t size = 0;
    char *dl_err = NULL;
    if (!download_to_memory(url, &data, &size, progress_cb, user_data, cancel_flag, &dl_err)) {
        if (dl_err) {
            if (err_msg) *err_msg = dl_err;
            else g_free(dl_err);
        } else if (err_msg) {
            *err_msg = NULL; /* cancelled */
        }
        return -1;
    }
    long result = fccdb_import_zip_bytes(db, data, size, is_daily, progress_cb, user_data, err_msg);
    g_free(data);
    return result;
}

/* ---- search / lookup ---- */

static FccRecord *record_from_stmt(sqlite3_stmt *s, gboolean with_unique_id) {
    FccRecord *r = g_new0(FccRecord, 1);
    int i = 0;
#define COL(dst) do { \
        const unsigned char *t = sqlite3_column_text(s, i++); \
        (dst) = g_strdup(t ? (const char *)t : ""); \
    } while (0)
    COL(r->call_sign); COL(r->entity_name); COL(r->first_name); COL(r->last_name);
    COL(r->street_address); COL(r->city); COL(r->state); COL(r->zip_code);
    COL(r->license_status); COL(r->operator_class); COL(r->grant_date); COL(r->expired_date);
    COL(r->effective_date); COL(r->cancellation_date);
    COL(r->entity_type); COL(r->applicant_type_code);
    COL(r->group_code); COL(r->trustee_call_sign);
    if (with_unique_id) { COL(r->trustee_name); COL(r->previous_call_sign); COL(r->frn); COL(r->unique_id); }
    else                { COL(r->previous_call_sign); COL(r->frn); r->trustee_name = g_strdup(""); r->unique_id = g_strdup(""); }
#undef COL
    return r;
}

GPtrArray *fccdb_search(FccDb *db, const FccSearchParams *p) {
    GPtrArray *out = g_ptr_array_new_with_free_func((GDestroyNotify)fcc_record_free);
    GString *sql = g_string_new(
        "SELECT call_sign, entity_name, first_name, last_name, "
        "street_address, city, state, zip_code, "
        "license_status, operator_class, grant_date, expired_date, "
        "effective_date, cancellation_date, "
        "entity_type, applicant_type_code, "
        "group_code, trustee_call_sign, previous_call_sign, frn "
        "FROM fcc_licenses");
    GPtrArray *clauses = g_ptr_array_new_with_free_func(g_free);
    GPtrArray *binds = g_ptr_array_new_with_free_func(g_free); /* strings to bind, in order */

    if (p->callsign && *p->callsign) {
        char *pattern = g_ascii_strup(p->callsign, -1);
        /* convert '*' wildcard to SQL '%' */
        for (char *c = pattern; *c; c++) if (*c == '*') *c = '%';
        if (!strchr(pattern, '%')) {
            char *withpct = g_strconcat(pattern, "%", NULL);
            g_free(pattern);
            pattern = withpct;
        }
        g_ptr_array_add(clauses, g_strdup("call_sign LIKE ?"));
        g_ptr_array_add(binds, pattern);
    }
    if (p->name && *p->name) {
        char *like = g_strconcat("%", p->name, "%", NULL);
        g_ptr_array_add(clauses, g_strdup("(entity_name LIKE ? OR first_name LIKE ? OR last_name LIKE ?)"));
        g_ptr_array_add(binds, g_strdup(like));
        g_ptr_array_add(binds, g_strdup(like));
        g_ptr_array_add(binds, like);
    }
    if (p->address && *p->address) {
        g_ptr_array_add(clauses, g_strdup("street_address LIKE ?"));
        g_ptr_array_add(binds, g_strconcat("%", p->address, "%", NULL));
    }
    if (p->city && *p->city) {
        g_ptr_array_add(clauses, g_strdup("city LIKE ?"));
        g_ptr_array_add(binds, g_strconcat("%", p->city, "%", NULL));
    }
    if (p->state && *p->state) {
        char *st = g_ascii_strup(p->state, -1);
        if (strlen(st) > 2) st[2] = '\0';
        g_ptr_array_add(clauses, g_strdup("state = ?"));
        g_ptr_array_add(binds, st);
    }
    if (p->op_class && *p->op_class) {
        char *oc = g_ascii_strup(p->op_class, -1);
        if (strlen(oc) > 1) oc[1] = '\0';
        g_ptr_array_add(clauses, g_strdup("operator_class = ?"));
        g_ptr_array_add(binds, oc);
    }
    if (p->status && *p->status) {
        char *st = g_ascii_strup(p->status, -1);
        if (strlen(st) > 1) st[1] = '\0';
        g_ptr_array_add(clauses, g_strdup("license_status = ?"));
        g_ptr_array_add(binds, st);
    }

    if (clauses->len > 0) {
        g_string_append(sql, " WHERE ");
        for (guint i = 0; i < clauses->len; i++) {
            if (i) g_string_append(sql, " AND ");
            g_string_append(sql, (const char *)g_ptr_array_index(clauses, i));
        }
    }
    g_string_append(sql, " ORDER BY call_sign LIMIT ?");

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->conn, sql->str, -1, &stmt, NULL);
    for (guint i = 0; i < binds->len; i++) {
        sqlite3_bind_text(stmt, (int)i + 1, (const char *)g_ptr_array_index(binds, i), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, (int)binds->len + 1, p->limit > 0 ? p->limit : 1000);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        g_ptr_array_add(out, record_from_stmt(stmt, FALSE));
    }
    sqlite3_finalize(stmt);

    g_string_free(sql, TRUE);
    g_ptr_array_free(clauses, TRUE);
    g_ptr_array_free(binds, TRUE);
    return out;
}

FccRecord *fccdb_lookup_callsign(FccDb *db, const char *callsign) {
    if (!callsign || !*callsign) return NULL;
    char *cs = g_ascii_strup(callsign, -1);
    g_strstrip(cs);
    const char *sql =
        "SELECT call_sign, entity_name, first_name, last_name, "
        "street_address, city, state, zip_code, "
        "license_status, operator_class, grant_date, expired_date, "
        "effective_date, cancellation_date, "
        "entity_type, applicant_type_code, "
        "group_code, trustee_call_sign, trustee_name, "
        "previous_call_sign, frn, unique_id "
        "FROM fcc_licenses WHERE call_sign = ? "
        "ORDER BY "
        "  CASE license_status WHEN 'A' THEN 0 ELSE 1 END ASC, "
        "  substr(effective_date,7,4)||substr(effective_date,1,2)||substr(effective_date,4,2) DESC, "
        "  substr(grant_date,7,4)||substr(grant_date,1,2)||substr(grant_date,4,2) DESC "
        "LIMIT 1";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
    FccRecord *r = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        r = record_from_stmt(stmt, TRUE);
    }
    sqlite3_finalize(stmt);
    g_free(cs);
    return r;
}

char *fccdb_get_last_import(FccDb *db) {
    sqlite3_stmt *stmt;
    char *result = NULL;
    sqlite3_prepare_v2(db->conn, "SELECT value FROM fcc_meta WHERE key='last_import'", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *t = sqlite3_column_text(stmt, 0);
        if (t) result = g_strdup((const char *)t);
    }
    sqlite3_finalize(stmt);
    return result;
}

long fccdb_get_record_count(FccDb *db) {
    sqlite3_stmt *stmt;
    long result = 0;
    sqlite3_prepare_v2(db->conn, "SELECT value FROM fcc_meta WHERE key='record_count'", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *t = sqlite3_column_text(stmt, 0);
        if (t) result = atol((const char *)t);
    }
    sqlite3_finalize(stmt);
    return result;
}
