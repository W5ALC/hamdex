#ifndef HAMDEX_DB_H
#define HAMDEX_DB_H

#include <sqlite3.h>
#include <glib.h>

typedef struct {
    sqlite3 *conn;
} HxDb;

typedef struct {
    char *callsign;
    char *name;   /* may be NULL */
    char *timestamp; /* ISO 8601 string */
} HxHistoryEntry;

HxDb *hxdb_open(const char *path); /* path may be NULL for default (~/.config/hamdex/hamdex.db) */
void  hxdb_close(HxDb *db);

void  hxdb_save_bookmark(HxDb *db, const char *callsign);
void  hxdb_remove_bookmark(HxDb *db, const char *callsign);
GPtrArray *hxdb_get_bookmarks(HxDb *db); /* array of g_strdup'd strings, most-recent-first */

void  hxdb_save_note(HxDb *db, const char *callsign, const char *note); /* empty note deletes */
char *hxdb_get_note(HxDb *db, const char *callsign); /* g_free() caller; NULL if none */

void  hxdb_add_history(HxDb *db, const char *callsign, const char *name);
GPtrArray *hxdb_get_history(HxDb *db, int limit); /* array of HxHistoryEntry*, most-recent-first */
void  hxdb_history_entry_free(HxHistoryEntry *e);
void  hxdb_clear_history(HxDb *db);

#endif
