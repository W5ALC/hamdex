/* ui_lookup.c — background-threaded callsign lookup (mirrors FCCLookupWorker) */
#include "hamdex_app.h"
#include <string.h>
#include <ctype.h>

typedef struct {
    HxApp *app;
    char *callsign;
} LookupTask;

typedef struct {
    HxApp *app;
    CallsignData *data; /* NULL on error */
    char *error;        /* NULL on success */
} LookupResult;

static const char *class_label(const char *code) {
    if (!code) return "";
    if (!strcmp(code, "T")) return "Technician";
    if (!strcmp(code, "G")) return "General";
    if (!strcmp(code, "A")) return "Advanced";
    if (!strcmp(code, "E")) return "Extra";
    return code;
}

static gboolean is_valid_callsign_format(const char *cs) {
    size_t n = strlen(cs);
    if (n < 3 || n > 8) return FALSE;
    for (size_t i = 0; i < n; i++) {
        char c = cs[i];
        if (!(isupper((unsigned char)c) || isdigit((unsigned char)c))) return FALSE;
    }
    return TRUE;
}

static void lookup_result_free(LookupResult *r) {
    if (!r) return;
    callsign_data_free(r->data);
    g_free(r->error);
    g_free(r);
}

static gboolean deliver_lookup_result(gpointer data) {
    LookupResult *r = data;
    HxApp *app = r->app;

    gtk_widget_set_sensitive(app->lookup_btn, TRUE);
    gtk_widget_set_sensitive(app->callsign_entry, TRUE);
    gtk_button_set_label(GTK_BUTTON(app->lookup_btn), "Lookup Callsign  [F5]");

    if (r->data) {
        hx_display_callsign(app, r->data);
        gtk_widget_set_sensitive(app->bookmark_btn, TRUE);
        char win_title[256];
        g_snprintf(win_title, sizeof(win_title), "HamDex - %s  (%s)",
                   r->data->callsign, r->data->name ? r->data->name : "Unknown");
        gtk_window_set_title(app->window, win_title);
        char status_msg[300];
        if (r->data->name)
            g_snprintf(status_msg, sizeof(status_msg), "Found %s  -  %s", r->data->callsign, r->data->name);
        else
            g_snprintf(status_msg, sizeof(status_msg), "Found %s", r->data->callsign);
        hx_show_status(app, status_msg, 5000);
        hxdb_add_history(app->db, r->data->callsign, r->data->name);
        hx_refresh_history(app);
    } else {
        GtkAlertDialog *alert = gtk_alert_dialog_new("%s", r->error ? r->error : "Lookup failed.");
        gtk_alert_dialog_set_modal(alert, TRUE);
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        hx_show_status(app, "Lookup failed", 5000);
        hx_clear_details(app);
        gtk_widget_set_sensitive(app->bookmark_btn, FALSE);
        gtk_window_set_title(app->window, "HamDex");
    }

    gtk_widget_grab_focus(app->callsign_entry);
    lookup_result_free(r);
    return G_SOURCE_REMOVE;
}

static gpointer lookup_thread_fn(gpointer data) {
    LookupTask *task = data;
    HxApp *app = task->app;
    LookupResult *result = g_new0(LookupResult, 1);
    result->app = app;

    char *cs = g_ascii_strup(task->callsign, -1);
    g_strstrip(cs);

    if (!*cs) {
        result->error = g_strdup("Callsign cannot be empty.");
    } else if (!is_valid_callsign_format(cs)) {
        result->error = g_strdup_printf("'%s' is not a valid callsign (3-8 alphanumeric characters).", cs);
    } else {
        FccRecord *row = fccdb_lookup_callsign(app->fcc_db, cs);
        if (!row) {
            result->error = g_strdup_printf(
                "Callsign '%s' not found in the local FCC database.", cs);
        } else {
            CallsignData *d = g_new0(CallsignData, 1);
            d->callsign = g_strdup(cs);
            char *fname = g_strstrip(g_strdup(row->first_name ? row->first_name : ""));
            char *lname = g_strstrip(g_strdup(row->last_name ? row->last_name : ""));
            if (*fname || *lname) {
                d->name = g_strstrip(g_strdup_printf("%s %s", fname, lname));
            } else if (row->entity_name && *row->entity_name) {
                d->name = g_strdup(row->entity_name);
            }
            g_free(fname); g_free(lname);

            GPtrArray *parts = g_ptr_array_new();
            if (row->street_address && *row->street_address) g_ptr_array_add(parts, row->street_address);
            if (row->city && *row->city) g_ptr_array_add(parts, row->city);
            if (row->state && *row->state) g_ptr_array_add(parts, row->state);
            if (row->zip_code && *row->zip_code) g_ptr_array_add(parts, row->zip_code);
            if (parts->len > 0) {
                GString *addr = g_string_new("");
                for (guint i = 0; i < parts->len; i++) {
                    if (i) g_string_append(addr, ", ");
                    g_string_append(addr, (const char *)g_ptr_array_index(parts, i));
                }
                d->address = g_string_free(addr, FALSE);
            }
            g_ptr_array_free(parts, TRUE);

            if (row->operator_class && *row->operator_class)
                d->operator_class = g_strdup(class_label(row->operator_class));
            if (row->expired_date && *row->expired_date) d->expiry_date = g_strdup(row->expired_date);
            if (row->license_status && *row->license_status) d->license_status = g_strdup(row->license_status);
            if (row->grant_date && *row->grant_date) d->grant_date = g_strdup(row->grant_date);
            if (row->previous_call_sign && *row->previous_call_sign)
                d->previous_call_sign = g_strdup(row->previous_call_sign);
            if (row->trustee_call_sign && *row->trustee_call_sign)
                d->trustee_call_sign = g_strdup(row->trustee_call_sign);
            if (row->trustee_name && *row->trustee_name)
                d->trustee_name = g_strdup(row->trustee_name);
            if (row->frn && *row->frn) d->frn = g_strdup(row->frn);
            if (row->unique_id && *row->unique_id) {
                d->fcc_url = g_strdup_printf(
                    "https://wireless2.fcc.gov/UlsApp/UlsSearch/license.jsp?licKey=%s", row->unique_id);
            }
            d->note = hxdb_get_note(app->db, cs);
            result->data = d;
            fcc_record_free(row);
        }
    }
    g_free(cs);
    g_idle_add(deliver_lookup_result, result);

    g_free(task->callsign);
    g_free(task);
    return NULL;
}

void hx_perform_lookup(HxApp *app, const char *callsign) {
    char *trimmed = g_strstrip(g_strdup(callsign));
    if (!*trimmed) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Please enter a callsign.");
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        g_free(trimmed);
        return;
    }
    gtk_widget_set_sensitive(app->lookup_btn, FALSE);
    gtk_widget_set_sensitive(app->callsign_entry, FALSE);
    gtk_button_set_label(GTK_BUTTON(app->lookup_btn), "Looking up...");
    char *status_msg = g_strdup_printf("Looking up %s...", trimmed);
    hx_show_status(app, status_msg, 0);
    g_free(status_msg);

    LookupTask *task = g_new0(LookupTask, 1);
    task->app = app;
    task->callsign = trimmed;
    GThread *thread = g_thread_new("lookup", lookup_thread_fn, task);
    g_thread_unref(thread);
}
