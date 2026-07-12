#ifndef HAMDEX_HAMDEX_APP_H
#define HAMDEX_HAMDEX_APP_H

#include <gtk/gtk.h>
#include "db.h"
#include "fcc_db.h"

/* In-memory result of a callsign lookup (mirrors the Python CallsignData). */
typedef struct {
    char *callsign;
    char *name;
    char *address;
    char *operator_class;
    char *expiry_date;
    char *license_status;
    char *fcc_url;
    char *note;
    char *grant_date;
    char *previous_call_sign;
    char *trustee_call_sign;
    char *trustee_name;
    char *frn;
} CallsignData;

void callsign_data_free(CallsignData *d);
CallsignData *callsign_data_copy(const CallsignData *d);

typedef struct HxApp {
    GtkApplication *gapp;
    GtkWindow *window;

    HxDb  *db;
    FccDb *fcc_db;

    gboolean dark_mode;
    char *home_grid;
    int font_size;

    GtkCssProvider *css_provider;

    /* Lookup tab widgets */
    GtkWidget *callsign_entry;
    GtkWidget *lookup_btn;
    GtkWidget *bookmark_btn;
    GtkWidget *bookmarks_list;
    GtkWidget *history_list;
    GtkWidget *font_spin;
    GtkWidget *dark_switch;
    GtkWidget *grid_entry;

    /* Details panel */
    GtkWidget *callsign_label;
    GtkWidget *operator_label;
    GtkWidget *status_badge;
    GtkWidget *info_cards_box;
    GtkWidget *notes_view;
    GtkWidget *map_btn, *qrz_btn, *fcc_btn, *export_btn;
    GtkWidget *details_notebook;

    CallsignData *current; /* owned, may be NULL */

    GtkWidget *statusbar;
    guint status_ctx;
    guint status_timeout_id;

    /* FCC tab widgets */
    GtkWidget *fcc_status_lbl;
    GtkWidget *fcc_daily_btn, *fcc_weekly_btn, *fcc_local_btn;
    GtkWidget *f_call, *f_name, *f_address, *f_city, *f_state;
    GtkWidget *f_class, *f_status;
    GtkWidget *result_lbl;
    GtkWidget *results_view;
    GtkListStore *results_store;
    GtkWidget *main_notebook;

    volatile int dl_cancel_flag;
    GtkWidget *progress_dialog;
    GtkWidget *progress_bar;
    GtkWidget *progress_label;
} HxApp;

/* Builds the whole window UI onto app->window (already created). */
void hx_build_ui(HxApp *app);

/* Settings persistence (GKeyFile at ~/.config/hamdex/settings.ini) */
void hx_load_settings(HxApp *app);
void hx_save_settings(HxApp *app);

/* status bar helper */
void hx_show_status(HxApp *app, const char *msg, int timeout_ms);

/* theme */
void hx_apply_theme(HxApp *app);

/* lookup flow */
void hx_perform_lookup(HxApp *app, const char *callsign);

/* refreshers */
void hx_refresh_bookmarks(HxApp *app);
void hx_refresh_history(HxApp *app);
void hx_display_callsign(HxApp *app, const CallsignData *data);
void hx_clear_details(HxApp *app);

/* FCC tab wiring (defined in ui_fcc_tab.c) */
void hx_fcc_tab_connect(HxApp *app);
void hx_fcc_search_connect(HxApp *app, GtkWidget *search_btn, GtkWidget *clear_btn);

/* FCC URL helpers (defined in fcc_urls.c) */
const char *hx_fcc_daily_url(void);
const char *hx_fcc_weekly_url(void);

#endif
