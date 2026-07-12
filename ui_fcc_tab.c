/* ui_fcc_tab.c — FCC ULS database tab: downloads, local import, search */
#include "hamdex_app.h"
#include <string.h>
#include <time.h>

static const char *STATUS_MAP(const char *s) {
    if (!s) return "";
    if (!strcmp(s, "A")) return "Active";
    if (!strcmp(s, "E")) return "Expired";
    if (!strcmp(s, "C")) return "Cancelled";
    if (!strcmp(s, "L")) return "Pending";
    return s;
}
static const char *CLASS_MAP(const char *s) {
    if (!s) return "";
    if (!strcmp(s, "T")) return "Technician";
    if (!strcmp(s, "G")) return "General";
    if (!strcmp(s, "A")) return "Advanced";
    if (!strcmp(s, "E")) return "Extra";
    return s;
}
static const char *ENTITY_MAP(const char *s) {
    if (!s || !*s) return "";
    if (!strcmp(s, "I")) return "Individual";
    if (!strcmp(s, "C")) return "Club";
    if (!strcmp(s, "M")) return "Military";
    if (!strcmp(s, "R")) return "RACES";
    if (!strcmp(s, "T")) return "Trust";
    return s;
}
static const char *GROUP_MAP(const char *s) {
    if (!s) return "";
    if (!strcmp(s, "A")) return "A (1x2/2x1)";
    if (!strcmp(s, "B")) return "B (1x3)";
    if (!strcmp(s, "C")) return "C (2x3 Tech)";
    if (!strcmp(s, "D")) return "D (2x3)";
    return s;
}

static void refresh_fcc_status(HxApp *app) {
    char *last = fccdb_get_last_import(app->fcc_db);
    long count = fccdb_get_record_count(app->fcc_db);
    char buf[256];
    if (last) {
        char ts[20]; g_strlcpy(ts, last, sizeof(ts));
        for (char *c = ts; *c; c++) if (*c == 'T') *c = ' ';
        g_snprintf(buf, sizeof(buf), "%'ld records  |  Last import: %s", count, ts);
    } else {
        g_strlcpy(buf, "No FCC data loaded — click a download button to get started.", sizeof(buf));
    }
    gtk_label_set_text(GTK_LABEL(app->fcc_status_lbl), buf);
    g_free(last);
}

static void set_fcc_buttons_enabled(HxApp *app, gboolean enabled) {
    gtk_widget_set_sensitive(app->fcc_daily_btn, enabled);
    gtk_widget_set_sensitive(app->fcc_weekly_btn, enabled);
    gtk_widget_set_sensitive(app->fcc_local_btn, enabled);
}

/* ---------------- progress dialog ---------------- */

static void on_progress_cancel(GtkButton *btn, gpointer user_data) {
    (void)btn;
    HxApp *app = user_data;
    app->dl_cancel_flag = 1;
}

static void show_progress_dialog(HxApp *app, const char *title, gboolean cancellable) {
    GtkWidget *dlg = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dlg), title);
    gtk_window_set_transient_for(GTK_WINDOW(dlg), app->window);
    gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 420, 120);
    gtk_window_set_deletable(GTK_WINDOW(dlg), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 16); gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16); gtk_widget_set_margin_bottom(box, 16);
    app->progress_label = gtk_label_new("Starting...");
    gtk_label_set_xalign(GTK_LABEL(app->progress_label), 0.0);
    app->progress_bar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(box), app->progress_label);
    gtk_box_append(GTK_BOX(box), app->progress_bar);
    if (cancellable) {
        GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
        g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_progress_cancel), app);
        gtk_box_append(GTK_BOX(box), cancel_btn);
    }
    gtk_window_set_child(GTK_WINDOW(dlg), box);
    app->progress_dialog = dlg;
    gtk_window_present(GTK_WINDOW(dlg));
}

static void close_progress_dialog(HxApp *app) {
    if (app->progress_dialog) {
        gtk_window_destroy(GTK_WINDOW(app->progress_dialog));
        app->progress_dialog = NULL;
        app->progress_bar = NULL;
        app->progress_label = NULL;
    }
}

typedef struct { HxApp *app; char *msg; int pct; } ProgressMsg;

static gboolean deliver_progress(gpointer data) {
    ProgressMsg *m = data;
    if (m->app->progress_label) gtk_label_set_text(GTK_LABEL(m->app->progress_label), m->msg);
    if (m->app->progress_bar) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m->app->progress_bar), m->pct / 100.0);
    g_free(m->msg);
    g_free(m);
    return G_SOURCE_REMOVE;
}

static void progress_cb(const char *msg, int pct, void *user_data) {
    HxApp *app = user_data;
    ProgressMsg *m = g_new0(ProgressMsg, 1);
    m->app = app; m->msg = g_strdup(msg); m->pct = pct;
    g_idle_add(deliver_progress, m);
}

/* ---------------- download / import worker ---------------- */

typedef struct {
    HxApp *app;
    char *url;      /* for network download; NULL if importing local file */
    char *path;     /* for local file import; NULL if network download */
    gboolean is_daily;
} ImportTask;

typedef struct {
    HxApp *app;
    long count;
    char *error; /* NULL on success; also NULL (no dialog) if user cancelled */
    gboolean cancelled;
} ImportResult;

static gboolean deliver_import_result(gpointer data) {
    ImportResult *r = data;
    HxApp *app = r->app;
    close_progress_dialog(app);
    set_fcc_buttons_enabled(app, TRUE);
    refresh_fcc_status(app);
    if (r->error) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Failed to import FCC data:\n\n%s", r->error);
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
    } else if (!r->cancelled) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Imported %'ld records from FCC ULS.", r->count);
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
    }
    g_free(r->error);
    g_free(r);
    return G_SOURCE_REMOVE;
}

static gpointer import_thread_fn(gpointer data) {
    ImportTask *task = data;
    HxApp *app = task->app;
    ImportResult *result = g_new0(ImportResult, 1);
    result->app = app;

    char *err = NULL;
    long count;
    if (task->url) {
        count = fccdb_download_and_import(app->fcc_db, task->url, task->is_daily,
                                           progress_cb, app, &app->dl_cancel_flag, &err);
    } else {
        count = fccdb_import_local_file(app->fcc_db, task->path, task->is_daily,
                                         progress_cb, app, &err);
    }
    if (count < 0) {
        if (err) result->error = err;
        else result->cancelled = TRUE;
    } else {
        result->count = count;
    }
    g_idle_add(deliver_import_result, result);
    g_free(task->url); g_free(task->path); g_free(task);
    return NULL;
}

static void start_import(HxApp *app, const char *url, const char *path, gboolean is_daily,
                          const char *dlg_title, gboolean cancellable) {
    app->dl_cancel_flag = 0;
    set_fcc_buttons_enabled(app, FALSE);
    show_progress_dialog(app, dlg_title, cancellable);
    ImportTask *task = g_new0(ImportTask, 1);
    task->app = app;
    task->url = url ? g_strdup(url) : NULL;
    task->path = path ? g_strdup(path) : NULL;
    task->is_daily = is_daily;
    GThread *t = g_thread_new("import", import_thread_fn, task);
    g_thread_unref(t);
}

static void on_daily_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    extern const char *hx_fcc_daily_url(void);
    start_import(app, hx_fcc_daily_url(), NULL, TRUE, "Downloading FCC ULS Data (Daily)", TRUE);
}

static void weekly_confirm_cb(GObject *src, GAsyncResult *res, gpointer user_data) {
    HxApp *app = user_data;
    GtkAlertDialog *alert = GTK_ALERT_DIALOG(src);
    GError *error = NULL;
    int idx = gtk_alert_dialog_choose_finish(alert, res, &error);
    if (error) { g_error_free(error); return; }
    if (idx == 0) { /* "Download" */
        extern const char *hx_fcc_weekly_url(void);
        start_import(app, hx_fcc_weekly_url(), NULL, FALSE, "Downloading FCC ULS Data (Full)", TRUE);
    }
}

static void on_weekly_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    GtkAlertDialog *alert = gtk_alert_dialog_new(
        "This downloads the full FCC amateur radio database (~100 MB).\nContinue?");
    const char *buttons[] = {"Download", "Cancel", NULL};
    gtk_alert_dialog_set_buttons(alert, buttons);
    gtk_alert_dialog_set_cancel_button(alert, 1);
    gtk_alert_dialog_set_default_button(alert, 0);
    gtk_alert_dialog_choose(alert, app->window, NULL, weekly_confirm_cb, app);
    g_object_unref(alert);
}

typedef struct { HxApp *app; char *path; } LocalImportCtx;

static void local_import_answer_cb(GObject *src, GAsyncResult *res, gpointer user_data) {
    LocalImportCtx *ctx = user_data;
    GtkAlertDialog *alert = GTK_ALERT_DIALOG(src);
    GError *error = NULL;
    int idx = gtk_alert_dialog_choose_finish(alert, res, &error);
    if (!error && idx != 2 /* not Cancel */) {
        gboolean is_daily = (idx == 0); /* "Yes, merge" */
        start_import(ctx->app, NULL, ctx->path, is_daily, "Importing Local FCC Data", FALSE);
    }
    if (error) g_error_free(error);
    g_free(ctx->path);
    g_free(ctx);
}

static void local_file_chosen_cb(GObject *src, GAsyncResult *res, gpointer user_data) {
    HxApp *app = user_data;
    GtkFileDialog *dlg = GTK_FILE_DIALOG(src);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_open_finish(dlg, res, &error);
    if (!file) { if (error) g_error_free(error); return; }
    char *path = g_file_get_path(file);
    g_object_unref(file);

    GtkAlertDialog *alert = gtk_alert_dialog_new(
        "Is this a daily update file?\n\nYes = merge into existing database\nNo = replace entire database (full weekly import)");
    const char *buttons[] = {"Yes, merge", "No, replace", "Cancel", NULL};
    gtk_alert_dialog_set_buttons(alert, buttons);
    gtk_alert_dialog_set_cancel_button(alert, 2);
    gtk_alert_dialog_set_default_button(alert, 0);
    LocalImportCtx *ctx = g_new0(LocalImportCtx, 1);
    ctx->app = app; ctx->path = path;
    gtk_alert_dialog_choose(alert, app->window, NULL, local_import_answer_cb, ctx);
    g_object_unref(alert);
}

static void on_local_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Open FCC ULS ZIP");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.zip");
    gtk_file_filter_set_name(filter, "ZIP Files");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);
    gtk_file_dialog_open(dlg, app->window, NULL, local_file_chosen_cb, app);
    g_object_unref(dlg);
}

/* ---------------- search ---------------- */

typedef struct {
    HxApp *app;
    FccSearchParams params;
} SearchTask;

typedef struct {
    HxApp *app;
    GPtrArray *rows; /* FccRecord* */
    char *error;
} SearchResult;

static gboolean deliver_search_result(gpointer data) {
    SearchResult *r = data;
    HxApp *app = r->app;
    if (r->error) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Search error: %s", r->error);
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        gtk_label_set_text(GTK_LABEL(app->result_lbl), "Search failed.");
    } else {
        gtk_list_store_clear(app->results_store);
        gboolean capped = r->rows->len >= 1000;
        char buf[64];
        if (capped)
            g_snprintf(buf, sizeof(buf), "1000+ (capped) result(s) — double-click to look up");
        else
            g_snprintf(buf, sizeof(buf), "%u result(s) — double-click to look up", r->rows->len);
        gtk_label_set_text(GTK_LABEL(app->result_lbl), buf);

        for (guint i = 0; i < r->rows->len; i++) {
            FccRecord *row = g_ptr_array_index(r->rows, i);
            char *fname = g_strstrip(g_strdup(row->first_name));
            char *lname = g_strstrip(g_strdup(row->last_name));
            char *name;
            if (*fname || *lname) name = g_strdup_printf("%s %s", fname, lname);
            else name = g_strdup(row->entity_name);
            g_strstrip(name);
            g_free(fname); g_free(lname);

            const char *raw_entity = (row->entity_type && *row->entity_type) ? row->entity_type : row->applicant_type_code;
            const char *entity = ENTITY_MAP(raw_entity);

            GtkTreeIter iter;
            gtk_list_store_append(app->results_store, &iter);
            gtk_list_store_set(app->results_store, &iter,
                0, row->call_sign, 1, name, 2, entity,
                3, row->street_address, 4, row->city, 5, row->state, 6, row->zip_code,
                7, CLASS_MAP(row->operator_class), 8, GROUP_MAP(row->group_code),
                9, STATUS_MAP(row->license_status),
                10, row->grant_date, 11, row->effective_date, 12, row->expired_date,
                13, row->cancellation_date, 14, row->trustee_call_sign, 15, row->previous_call_sign,
                -1);
            g_free(name);
        }
    }
    g_free(r->error);
    /* r->rows was created with fcc_record_free as its free_func, so
     * g_ptr_array_free(..., TRUE) below already frees every element —
     * do not free them here too. */
    g_ptr_array_free(r->rows, TRUE);
    g_free(r);
    return G_SOURCE_REMOVE;
}

static gpointer search_thread_fn(gpointer data) {
    SearchTask *task = data;
    HxApp *app = task->app;
    SearchResult *result = g_new0(SearchResult, 1);
    result->app = app;
    result->rows = fccdb_search(app->fcc_db, &task->params);

    g_free(task->params.callsign); g_free(task->params.name); g_free(task->params.address);
    g_free(task->params.city); g_free(task->params.state);
    g_free(task->params.op_class); g_free(task->params.status);
    g_free(task);

    g_idle_add(deliver_search_result, result);
    return NULL;
}

static char *entry_text_dup(GtkWidget *entry) {
    return g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
}

static void do_search(HxApp *app) {
    char *call = g_strstrip(entry_text_dup(app->f_call));
    char *name = g_strstrip(entry_text_dup(app->f_name));
    char *addr = g_strstrip(entry_text_dup(app->f_address));
    char *city = g_strstrip(entry_text_dup(app->f_city));
    char *state = g_strstrip(entry_text_dup(app->f_state));

    const char *class_sel = gtk_string_object_get_string(
        GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(app->f_class))));
    const char *status_sel = gtk_string_object_get_string(
        GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(GTK_DROP_DOWN(app->f_status))));

    char op_class[2] = {0}, status[2] = {0};
    if (class_sel && strcmp(class_sel, "Any") != 0) op_class[0] = class_sel[0];
    if (status_sel && strcmp(status_sel, "Any") != 0) status[0] = status_sel[0];

    if (!*call && !*name && !*addr && !*city && !*state && !op_class[0] && !status[0]) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Please enter at least one search filter.");
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        g_free(call); g_free(name); g_free(addr); g_free(city); g_free(state);
        return;
    }

    gtk_label_set_text(GTK_LABEL(app->result_lbl), "Searching...");
    gtk_list_store_clear(app->results_store);

    SearchTask *task = g_new0(SearchTask, 1);
    task->app = app;
    task->params.callsign = call;
    task->params.name = name;
    task->params.address = addr;
    task->params.city = city;
    task->params.state = state;
    task->params.op_class = g_strdup(op_class);
    task->params.status = g_strdup(status);
    task->params.limit = 1000;

    GThread *t = g_thread_new("search", search_thread_fn, task);
    g_thread_unref(t);
}

static void on_search_clicked(GtkButton *b, gpointer user_data) { (void)b; do_search(user_data); }
static void on_filter_activate(GtkEntry *e, gpointer user_data) { (void)e; do_search(user_data); }

static void on_clear_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    gtk_editable_set_text(GTK_EDITABLE(app->f_call), "");
    gtk_editable_set_text(GTK_EDITABLE(app->f_name), "");
    gtk_editable_set_text(GTK_EDITABLE(app->f_address), "");
    gtk_editable_set_text(GTK_EDITABLE(app->f_city), "");
    gtk_editable_set_text(GTK_EDITABLE(app->f_state), "");
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app->f_class), 0);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app->f_status), 0);
    gtk_list_store_clear(app->results_store);
    gtk_label_set_text(GTK_LABEL(app->result_lbl), "Filters cleared.");
}

static void on_row_activated(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data) {
    (void)col;
    HxApp *app = user_data;
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(view);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *callsign = NULL;
        gtk_tree_model_get(model, &iter, 0, &callsign, -1);
        if (callsign) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(app->main_notebook), 0);
            gtk_editable_set_text(GTK_EDITABLE(app->callsign_entry), callsign);
            hx_perform_lookup(app, callsign);
            g_free(callsign);
        }
    }
}

/* public entry points used by ui_build.c */
void hx_fcc_tab_connect(HxApp *app) {
    g_signal_connect(app->fcc_daily_btn, "clicked", G_CALLBACK(on_daily_clicked), app);
    g_signal_connect(app->fcc_weekly_btn, "clicked", G_CALLBACK(on_weekly_clicked), app);
    g_signal_connect(app->fcc_local_btn, "clicked", G_CALLBACK(on_local_clicked), app);
    g_signal_connect(app->results_view, "row-activated", G_CALLBACK(on_row_activated), app);
    refresh_fcc_status(app);
}
void hx_fcc_search_connect(HxApp *app, GtkWidget *search_btn, GtkWidget *clear_btn) {
    g_signal_connect(search_btn, "clicked", G_CALLBACK(on_search_clicked), app);
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_clear_clicked), app);
    g_signal_connect(app->f_call, "activate", G_CALLBACK(on_filter_activate), app);
    g_signal_connect(app->f_name, "activate", G_CALLBACK(on_filter_activate), app);
    g_signal_connect(app->f_address, "activate", G_CALLBACK(on_filter_activate), app);
    g_signal_connect(app->f_city, "activate", G_CALLBACK(on_filter_activate), app);
    g_signal_connect(app->f_state, "activate", G_CALLBACK(on_filter_activate), app);
}
