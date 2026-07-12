#ifndef HAMDEX_FCC_DB_H
#define HAMDEX_FCC_DB_H

#include <sqlite3.h>
#include <glib.h>
#include "download.h"

typedef struct {
    sqlite3 *conn;
} FccDb;

/* One search result / lookup result row. All strings owned, g_free() each
 * plus fcc_record_free(). NULL/empty string means "field absent". */
typedef struct {
    char *call_sign, *entity_name, *first_name, *last_name;
    char *street_address, *city, *state, *zip_code;
    char *license_status, *operator_class, *grant_date, *expired_date;
    char *effective_date, *cancellation_date;
    char *entity_type, *applicant_type_code;
    char *group_code, *trustee_call_sign, *trustee_name, *previous_call_sign;
    char *frn, *unique_id;
} FccRecord;

typedef struct {
    char *callsign; /* required; "*" wildcard allowed like "W1*" */
    char *name;
    char *address;
    char *city;
    char *state;
    char *op_class;   /* single letter: T/G/A/E or empty */
    char *status;     /* single letter: A/E/C/L or empty */
    int limit;
} FccSearchParams;

FccDb *fccdb_open(const char *path); /* NULL => default ~/.config/hamdex/fcc_uls.db */
void   fccdb_close(FccDb *db);

void   fcc_record_free(FccRecord *r);

/* Imports FCC zip bytes already in memory. If is_daily, merges (INSERT OR
 * REPLACE) into existing data; otherwise replaces the whole table first.
 * Returns number of records written, or -1 on error (sets *err_msg). */
long   fccdb_import_zip_bytes(FccDb *db, const unsigned char *data, size_t size,
                               gboolean is_daily,
                               download_progress_cb progress_cb, void *user_data,
                               char **err_msg);

/* Convenience: reads a local file into memory then imports it. */
long   fccdb_import_local_file(FccDb *db, const char *path, gboolean is_daily,
                                download_progress_cb progress_cb, void *user_data,
                                char **err_msg);

/* Convenience: downloads then imports. */
long   fccdb_download_and_import(FccDb *db, const char *url, gboolean is_daily,
                                  download_progress_cb progress_cb, void *user_data,
                                  volatile int *cancel_flag, char **err_msg);

/* Runs a filtered search. Returns a GPtrArray of FccRecord* (caller frees
 * each with fcc_record_free then g_ptr_array_free). */
GPtrArray *fccdb_search(FccDb *db, const FccSearchParams *params);

/* Exact callsign lookup (active license preferred). Returns NULL if not
 * found. Caller frees with fcc_record_free(). */
FccRecord *fccdb_lookup_callsign(FccDb *db, const char *callsign);

char  *fccdb_get_last_import(FccDb *db); /* g_free(); NULL if never imported */
long   fccdb_get_record_count(FccDb *db);

#endif
