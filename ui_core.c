/* ui_core.c — theme/settings/status bar/details panel/lookup flow */
#include "hamdex_app.h"
#include "app.h"
#include <string.h>
#include <stdio.h>

/* ============================ CallsignData ============================ */

void callsign_data_free(CallsignData *d) {
    if (!d) return;
    g_free(d->callsign); g_free(d->name); g_free(d->address);
    g_free(d->operator_class); g_free(d->expiry_date);
    g_free(d->license_status); g_free(d->fcc_url); g_free(d->note);
    g_free(d->grant_date);
    g_free(d->previous_call_sign); g_free(d->trustee_call_sign); g_free(d->trustee_name);
    g_free(d->frn);
    g_free(d);
}

CallsignData *callsign_data_copy(const CallsignData *s) {
    if (!s) return NULL;
    CallsignData *d = g_new0(CallsignData, 1);
    d->callsign = g_strdup(s->callsign);
    d->name = s->name ? g_strdup(s->name) : NULL;
    d->address = s->address ? g_strdup(s->address) : NULL;
    d->operator_class = s->operator_class ? g_strdup(s->operator_class) : NULL;
    d->expiry_date = s->expiry_date ? g_strdup(s->expiry_date) : NULL;
    d->license_status = s->license_status ? g_strdup(s->license_status) : NULL;
    d->fcc_url = s->fcc_url ? g_strdup(s->fcc_url) : NULL;
    d->note = s->note ? g_strdup(s->note) : NULL;
    d->grant_date = s->grant_date ? g_strdup(s->grant_date) : NULL;
    d->previous_call_sign = s->previous_call_sign ? g_strdup(s->previous_call_sign) : NULL;
    d->trustee_call_sign = s->trustee_call_sign ? g_strdup(s->trustee_call_sign) : NULL;
    d->trustee_name = s->trustee_name ? g_strdup(s->trustee_name) : NULL;
    d->frn = s->frn ? g_strdup(s->frn) : NULL;
    return d;
}

/* ============================== Theme CSS ============================== */

static const char *DARK_CSS =
"window { background-color: #000000; }"
"box.leftpanel, box.detailspanel { background-color: #000000; }"
"entry { background-color: #313244; color: #cdd6f4; border-radius: 8px; padding: 8px; }"
"button { background-color: #585b70; color: #cdd6f4; border-radius: 8px; padding: 8px 14px; }"
"button:hover { background-color: #89b4fa; color: #1e1e2e; }"
"button.primary { background-color: #89b4fa; color: #1e1e2e; font-weight: bold; }"
"button.primary:hover { background-color: #b4d0fb; }"
"button.accent { background-color: #f38ba8; color: #1e1e2e; }"
"button.accent:hover { background-color: #f5a3b5; }"
"frame, .card { background-color: #181825; border-radius: 8px; }"
"frame > label { color: #cdd6f4; }"
"label { color: #cdd6f4; }"
"label.dim { color: #a6adc8; }"
"label.callsign { font-size: 28pt; font-weight: bold; color: #89b4fa; }"
"label.badge-active { color: #a6e3a1; background-color: #0d2b1a; border-radius: 8px; padding: 4px 16px; }"
"label.badge-warn { color: #f9e2af; background-color: #2e2000; border-radius: 8px; padding: 4px 16px; }"
"label.badge-bad { color: #f38ba8; background-color: #3b1219; border-radius: 8px; padding: 4px 16px; }"
"treeview, textview, list { background-color: #181825; color: #cdd6f4; }"
"treeview row:selected, list row:selected { background-color: #89b4fa; color: #1e1e2e; }"
"notebook > header { background-color: #181825; }"
"notebook > header tab { background-color: #313244; color: #cdd6f4; }"
"notebook > header tab:checked { background-color: #89b4fa; color: #1e1e2e; }"
"notebook > stack { background-color: #000000; }"
"scrolledwindow, scrolledwindow viewport, viewport { background-color: transparent; }"
"textview text { background-color: #181825; color: #cdd6f4; }"
"entry text, spinbutton text { background-color: transparent; color: inherit; }"
"headerbar { background-color: #11111b; color: #cdd6f4; border-bottom: 1px solid #313244; }"
"headerbar button { background-color: transparent; }"
"headerbar button:hover { background-color: #313244; color: #cdd6f4; }"
"switch { background-color: #45475a; border-radius: 999px; }"
"switch:checked { background-color: #89b4fa; }"
"switch slider { background-color: #cdd6f4; border-radius: 999px; }"
"dropdown, dropdown button { background-color: #313244; color: #cdd6f4; border-radius: 8px; }"
"dropdown popover, popover { background-color: #181825; color: #cdd6f4; border-radius: 8px; }"
"popover contents { background-color: #181825; }"
"spinbutton { background-color: #313244; color: #cdd6f4; border-radius: 8px; }"
"spinbutton button { background-color: #45475a; color: #cdd6f4; }"
"scrollbar slider { background-color: #585b70; border-radius: 999px; }"
"scrollbar slider:hover { background-color: #89b4fa; }"
"menu, menubutton popover { background-color: #181825; color: #cdd6f4; }"
"paned > separator { background-color: #313244; }";

static const char *LIGHT_CSS =
"window { background-color: #f5f5f5; }"
"box.leftpanel, box.detailspanel { background-color: #f5f5f5; }"
"entry { background-color: #ffffff; color: #2c3e50; border-radius: 8px; padding: 8px; border: 1px solid #d0d5dd; }"
"button { background-color: #e8edf2; color: #2c3e50; border-radius: 8px; padding: 8px 14px; }"
"button:hover { background-color: #3498db; color: #ffffff; }"
"button.primary { background-color: #3498db; color: #ffffff; font-weight: bold; }"
"button.primary:hover { background-color: #5dade2; }"
"button.accent { background-color: #e74c3c; color: #ffffff; }"
"button.accent:hover { background-color: #ec7063; }"
"frame, .card { background-color: #ffffff; border-radius: 8px; border: 1px solid #d0d5dd; }"
"frame > label { color: #2c3e50; }"
"label { color: #2c3e50; }"
"label.dim { color: #7f8c8d; }"
"label.callsign { font-size: 28pt; font-weight: bold; color: #2980b9; }"
"label.badge-active { color: #196f3d; background-color: #d5f5e3; border-radius: 8px; padding: 4px 16px; }"
"label.badge-warn { color: #7d6608; background-color: #fdebd0; border-radius: 8px; padding: 4px 16px; }"
"label.badge-bad { color: #922b21; background-color: #fadbd8; border-radius: 8px; padding: 4px 16px; }"
"treeview, textview, list { background-color: #ffffff; color: #2c3e50; }"
"treeview row:selected, list row:selected { background-color: #3498db; color: #ffffff; }"
"notebook > header { background-color: #ffffff; }"
"notebook > header tab { background-color: #ecf0f1; color: #2c3e50; }"
"notebook > header tab:checked { background-color: #3498db; color: #ffffff; }"
"notebook > stack { background-color: #f5f5f5; }"
"scrolledwindow, scrolledwindow viewport, viewport { background-color: transparent; }"
"textview text { background-color: #ffffff; color: #2c3e50; }"
"entry text, spinbutton text { background-color: transparent; color: inherit; }"
"headerbar { background-color: #ffffff; color: #2c3e50; border-bottom: 1px solid #d0d5dd; }"
"headerbar button { background-color: transparent; }"
"headerbar button:hover { background-color: #e8edf2; color: #2c3e50; }"
"switch { background-color: #d0d5dd; border-radius: 999px; }"
"switch:checked { background-color: #3498db; }"
"switch slider { background-color: #ffffff; border-radius: 999px; }"
"dropdown, dropdown button { background-color: #ffffff; color: #2c3e50; border-radius: 8px; border: 1px solid #d0d5dd; }"
"dropdown popover, popover { background-color: #ffffff; color: #2c3e50; border-radius: 8px; border: 1px solid #d0d5dd; }"
"popover contents { background-color: #ffffff; }"
"spinbutton { background-color: #ffffff; color: #2c3e50; border-radius: 8px; border: 1px solid #d0d5dd; }"
"spinbutton button { background-color: #e8edf2; color: #2c3e50; }"
"scrollbar slider { background-color: #bfc9d4; border-radius: 999px; }"
"scrollbar slider:hover { background-color: #3498db; }"
"menu, menubutton popover { background-color: #ffffff; color: #2c3e50; }"
"paned > separator { background-color: #d0d5dd; }";

void hx_apply_theme(HxApp *app) {
    /* Combine the base light/dark stylesheet with a font-size override so a
     * single provider (app->css_provider) always reflects both settings.
     * Folding font size in here means loading it at startup (or changing it
     * later) always takes effect immediately, instead of only applying once
     * the user touches the font-size spin button. */
    GString *css = g_string_new(app->dark_mode ? DARK_CSS : LIGHT_CSS);
    g_string_append_printf(css, "window { font-size: %dpt; }", app->font_size);
    gtk_css_provider_load_from_string(app->css_provider, css->str);
    g_string_free(css, TRUE);
    hx_show_status(app, app->dark_mode ? "Dark mode enabled" : "Light mode enabled", 4000);
}

/* ============================== Settings ============================== */

static char *settings_path(void) {
    const char *home = g_get_home_dir();
    char *dir = g_build_filename(home, ".config", "hamdex", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *path = g_build_filename(dir, "settings.ini", NULL);
    g_free(dir);
    return path;
}

void hx_load_settings(HxApp *app) {
    char *path = settings_path();
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    app->dark_mode = TRUE;
    app->font_size = 16;
    app->home_grid = g_strdup("");
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err)) {
        app->dark_mode = g_key_file_get_boolean(kf, "hamdex", "dark_mode", NULL);
        int fs = g_key_file_get_integer(kf, "hamdex", "font_size", NULL);
        if (fs >= 8 && fs <= 20) app->font_size = fs;
        char *grid = g_key_file_get_string(kf, "hamdex", "home_grid", NULL);
        if (grid) { g_free(app->home_grid); app->home_grid = grid; }
    } else if (err) {
        g_error_free(err);
    }
    g_key_file_free(kf);
    g_free(path);
}

void hx_save_settings(HxApp *app) {
    char *path = settings_path();
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_boolean(kf, "hamdex", "dark_mode", app->dark_mode);
    g_key_file_set_integer(kf, "hamdex", "font_size", app->font_size);
    g_key_file_set_string(kf, "hamdex", "home_grid", app->home_grid ? app->home_grid : "");
    GError *err = NULL;
    if (!g_key_file_save_to_file(kf, path, &err)) {
        g_printerr("hamdex: could not save settings: %s\n", err->message);
        g_error_free(err);
    }
    g_key_file_free(kf);
    g_free(path);
}

/* ============================== Status bar ============================== */

static gboolean clear_status(gpointer data) {
    HxApp *app = data;
    gtk_label_set_text(GTK_LABEL(app->statusbar), "");
    app->status_timeout_id = 0;
    return G_SOURCE_REMOVE;
}

void hx_show_status(HxApp *app, const char *msg, int timeout_ms) {
    if (!app->statusbar) return;
    gtk_label_set_text(GTK_LABEL(app->statusbar), msg);
    if (app->status_timeout_id) g_source_remove(app->status_timeout_id);
    app->status_timeout_id = 0;
    if (timeout_ms > 0) {
        app->status_timeout_id = g_timeout_add(timeout_ms, clear_status, app);
    }
}

/* ============================== Details panel ============================== */

static void clear_box_children(GtkWidget *box) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(box)) != NULL) {
        gtk_box_remove(GTK_BOX(box), child);
    }
}

static GtkWidget *make_card(const char *title, const char *value) {
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_add_css_class(frame, "card");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);
    GtkWidget *t = gtk_label_new(title);
    gtk_widget_add_css_class(t, "dim");
    gtk_label_set_xalign(GTK_LABEL(t), 0.0);
    GtkWidget *v = gtk_label_new(value);
    gtk_label_set_xalign(GTK_LABEL(v), 0.0);
    gtk_label_set_wrap(GTK_LABEL(v), TRUE);
    gtk_label_set_selectable(GTK_LABEL(v), TRUE);
    gtk_box_append(GTK_BOX(box), t);
    gtk_box_append(GTK_BOX(box), v);
    gtk_frame_set_child(GTK_FRAME(frame), box);
    return frame;
}

void hx_clear_details(HxApp *app) {
    if (app->current) { callsign_data_free(app->current); app->current = NULL; }
    gtk_label_set_text(GTK_LABEL(app->callsign_label), "No Data");
    gtk_label_set_text(GTK_LABEL(app->operator_label), "");
    gtk_widget_set_visible(app->status_badge, FALSE);
    clear_box_children(app->info_cards_box);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->notes_view));
    gtk_text_buffer_set_text(buf, "", -1);
    gtk_widget_set_sensitive(app->map_btn, FALSE);
    gtk_widget_set_sensitive(app->qrz_btn, FALSE);
    gtk_widget_set_sensitive(app->fcc_btn, FALSE);
    gtk_widget_set_sensitive(app->export_btn, FALSE);
}

void hx_display_callsign(HxApp *app, const CallsignData *data) {
    CallsignData *copy = callsign_data_copy(data);
    if (app->current) callsign_data_free(app->current);
    app->current = copy;
    CallsignData *d = app->current;

    gtk_label_set_text(GTK_LABEL(app->callsign_label), d->callsign ? d->callsign : "Unknown");
    gtk_label_set_text(GTK_LABEL(app->operator_label), d->name ? d->name : "");

    gtk_widget_remove_css_class(app->status_badge, "badge-active");
    gtk_widget_remove_css_class(app->status_badge, "badge-warn");
    gtk_widget_remove_css_class(app->status_badge, "badge-bad");
    const char *status = d->license_status ? d->license_status : "";
    if (g_strcmp0(status, "E") == 0) {
        gtk_label_set_text(GTK_LABEL(app->status_badge), "LICENSE EXPIRED");
        gtk_widget_add_css_class(app->status_badge, "badge-bad");
    } else if (g_strcmp0(status, "C") == 0) {
        gtk_label_set_text(GTK_LABEL(app->status_badge), "LICENSE CANCELLED");
        gtk_widget_add_css_class(app->status_badge, "badge-bad");
    } else if (g_strcmp0(status, "L") == 0) {
        gtk_label_set_text(GTK_LABEL(app->status_badge), "LICENSE PENDING");
        gtk_widget_add_css_class(app->status_badge, "badge-warn");
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_badge), "ACTIVE");
        gtk_widget_add_css_class(app->status_badge, "badge-active");
    }
    gtk_widget_set_visible(app->status_badge, TRUE);

    clear_box_children(app->info_cards_box);
    if (d->note && *d->note)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("Personal Note", d->note));
    if (d->operator_class && *d->operator_class)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("License Class", d->operator_class));
    if (d->address && *d->address)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("Address", d->address));
    if (d->grant_date && *d->grant_date)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("Grant Date", d->grant_date));
    if (d->expiry_date && *d->expiry_date)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("License Expiry", d->expiry_date));
    if (d->previous_call_sign && *d->previous_call_sign)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("Previous Callsign", d->previous_call_sign));
    if (d->trustee_call_sign && *d->trustee_call_sign) {
        char *trustee = d->trustee_name && *d->trustee_name
            ? g_strdup_printf("%s (%s)", d->trustee_call_sign, d->trustee_name)
            : g_strdup(d->trustee_call_sign);
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("Trustee", trustee));
        g_free(trustee);
    }
    if (d->frn && *d->frn)
        gtk_box_append(GTK_BOX(app->info_cards_box), make_card("FRN", d->frn));

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->notes_view));
    gtk_text_buffer_set_text(buf, d->note ? d->note : "", -1);

    gboolean has_map_target = d->address && *d->address;
    gtk_widget_set_sensitive(app->map_btn, has_map_target);
    gtk_widget_set_sensitive(app->qrz_btn, TRUE);
    gtk_widget_set_sensitive(app->fcc_btn, TRUE);
    gtk_widget_set_sensitive(app->export_btn, TRUE);
}

/* ============================== Refresh lists ============================== */

void hx_refresh_bookmarks(HxApp *app) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(app->bookmarks_list)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(app->bookmarks_list), child);
    GPtrArray *bms = hxdb_get_bookmarks(app->db);
    for (guint i = 0; i < bms->len; i++) {
        const char *cs = g_ptr_array_index(bms, i);
        GtkWidget *row = gtk_label_new(cs);
        gtk_label_set_xalign(GTK_LABEL(row), 0.0);
        gtk_widget_set_margin_start(row, 8);
        gtk_widget_set_margin_top(row, 4);
        gtk_widget_set_margin_bottom(row, 4);
        gtk_list_box_append(GTK_LIST_BOX(app->bookmarks_list), row);
    }
    g_ptr_array_free(bms, TRUE);
}

void hx_refresh_history(HxApp *app) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(app->history_list)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(app->history_list), child);
    GPtrArray *hist = hxdb_get_history(app->db, 20);
    for (guint i = 0; i < hist->len; i++) {
        HxHistoryEntry *e = g_ptr_array_index(hist, i);
        char *ts_short = g_strndup(e->timestamp, 16);
        char *label = g_strdup_printf("%-8s  %s  %s", e->callsign, ts_short, e->name ? e->name : "");
        GtkWidget *row = gtk_label_new(label);
        gtk_label_set_xalign(GTK_LABEL(row), 0.0);
        gtk_widget_set_margin_start(row, 8);
        gtk_widget_set_margin_top(row, 4);
        gtk_widget_set_margin_bottom(row, 4);
        gtk_list_box_append(GTK_LIST_BOX(app->history_list), row);
        g_free(label);
        g_free(ts_short);
    }
    g_ptr_array_free(hist, TRUE);
}
