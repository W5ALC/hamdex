#include "db.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static char *default_db_path(void) {
    const char *home = g_get_home_dir();
    char *dir = g_build_filename(home, ".config", "hamdex", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *path = g_build_filename(dir, "hamdex.db", NULL);
    g_free(dir);
    return path;
}

static char *now_iso8601(void) {
    GDateTime *dt = g_date_time_new_now_local();
    char *s = g_date_time_format(dt, "%Y-%m-%dT%H:%M:%S");
    g_date_time_unref(dt);
    return s;
}

static char *to_upper(const char *s) {
    char *u = g_ascii_strup(s ? s : "", -1);
    return u;
}

HxDb *hxdb_open(const char *path) {
    HxDb *db = g_new0(HxDb, 1);
    char *p = path ? g_strdup(path) : default_db_path();
    if (sqlite3_open(p, &db->conn) != SQLITE_OK) {
        g_printerr("hxdb: failed to open %s: %s\n", p, sqlite3_errmsg(db->conn));
    }
    sqlite3_exec(db->conn, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    const char *schema =
        "CREATE TABLE IF NOT EXISTS bookmarks ("
        "  callsign TEXT PRIMARY KEY,"
        "  added_date TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS notes ("
        "  callsign TEXT PRIMARY KEY,"
        "  note TEXT NOT NULL,"
        "  updated_date TEXT DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS lookup_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  callsign TEXT NOT NULL,"
        "  name TEXT,"
        "  timestamp TEXT NOT NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_history_ts ON lookup_history(timestamp DESC);";
    char *errmsg = NULL;
    if (sqlite3_exec(db->conn, schema, NULL, NULL, &errmsg) != SQLITE_OK) {
        g_printerr("hxdb: schema init failed: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    g_free(p);
    return db;
}

void hxdb_close(HxDb *db) {
    if (!db) return;
    if (db->conn) sqlite3_close(db->conn);
    g_free(db);
}

void hxdb_save_bookmark(HxDb *db, const char *callsign) {
    char *cs = to_upper(callsign);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->conn, "INSERT OR IGNORE INTO bookmarks (callsign) VALUES (?)", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    g_free(cs);
}

void hxdb_remove_bookmark(HxDb *db, const char *callsign) {
    char *cs = to_upper(callsign);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->conn, "DELETE FROM bookmarks WHERE callsign=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    g_free(cs);
}

GPtrArray *hxdb_get_bookmarks(HxDb *db) {
    GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->conn, "SELECT callsign FROM bookmarks ORDER BY added_date DESC", -1, &stmt, NULL);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        g_ptr_array_add(arr, g_strdup((const char *)sqlite3_column_text(stmt, 0)));
    }
    sqlite3_finalize(stmt);
    return arr;
}

void hxdb_save_note(HxDb *db, const char *callsign, const char *note) {
    char *cs = to_upper(callsign);
    char *trimmed = note ? g_strstrip(g_strdup(note)) : g_strdup("");
    if (trimmed[0] != '\0') {
        char *ts = now_iso8601();
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db->conn,
            "INSERT OR REPLACE INTO notes (callsign, note, updated_date) VALUES (?,?,?)",
            -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, trimmed, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, ts, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        g_free(ts);
    } else {
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db->conn, "DELETE FROM notes WHERE callsign=?", -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    g_free(cs);
    g_free(trimmed);
}

char *hxdb_get_note(HxDb *db, const char *callsign) {
    char *cs = to_upper(callsign);
    sqlite3_stmt *stmt;
    char *result = NULL;
    sqlite3_prepare_v2(db->conn, "SELECT note FROM notes WHERE callsign=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *txt = sqlite3_column_text(stmt, 0);
        if (txt) result = g_strdup((const char *)txt);
    }
    sqlite3_finalize(stmt);
    g_free(cs);
    return result;
}

void hxdb_add_history(HxDb *db, const char *callsign, const char *name) {
    char *cs = to_upper(callsign);
    char *ts = now_iso8601();
    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(db->conn, "DELETE FROM lookup_history WHERE callsign=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db->conn,
        "INSERT INTO lookup_history (callsign, name, timestamp) VALUES (?,?,?)",
        -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, cs, -1, SQLITE_TRANSIENT);
    if (name) sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    else      sqlite3_bind_null(stmt, 2);
    sqlite3_bind_text(stmt, 3, ts, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_exec(db->conn,
        "DELETE FROM lookup_history WHERE id NOT IN ("
        "  SELECT id FROM lookup_history ORDER BY timestamp DESC LIMIT 100)",
        NULL, NULL, NULL);

    g_free(cs);
    g_free(ts);
}

void hxdb_history_entry_free(HxHistoryEntry *e) {
    if (!e) return;
    g_free(e->callsign);
    g_free(e->name);
    g_free(e->timestamp);
    g_free(e);
}

GPtrArray *hxdb_get_history(HxDb *db, int limit) {
    GPtrArray *arr = g_ptr_array_new_with_free_func((GDestroyNotify)hxdb_history_entry_free);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->conn,
        "SELECT callsign, name, timestamp FROM lookup_history ORDER BY timestamp DESC LIMIT ?",
        -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HxHistoryEntry *e = g_new0(HxHistoryEntry, 1);
        e->callsign = g_strdup((const char *)sqlite3_column_text(stmt, 0));
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        e->name = name ? g_strdup((const char *)name) : NULL;
        e->timestamp = g_strdup((const char *)sqlite3_column_text(stmt, 2));
        g_ptr_array_add(arr, e);
    }
    sqlite3_finalize(stmt);
    return arr;
}

void hxdb_clear_history(HxDb *db) {
    sqlite3_exec(db->conn, "DELETE FROM lookup_history", NULL, NULL, NULL);
}
