/* ui_build.c — builds the window, wires action buttons/shortcuts, main() */
#include "hamdex_app.h"
#include "app.h"
#include <string.h>
#include <stdio.h>
#include <json-glib/json-glib.h>

/* ============================== helpers ============================== */

static void open_uri(HxApp *app, const char *uri) {
    GError *err = NULL;
    if (!g_app_info_launch_default_for_uri(uri, NULL, &err)) {
        char *msg = g_strdup_printf("Could not open link: %s",
                                     err ? err->message : "no default application found");
        hx_show_status(app, msg, 6000);
        g_free(msg);
        if (err) g_error_free(err);
    }
}

static char *url_encode(const char *s) {
    return g_uri_escape_string(s, NULL, FALSE);
}

/* ============================== callsign entry ============================== */

static gboolean g_uppercasing = FALSE;

static void on_callsign_changed(GtkEditable *editable, gpointer user_data) {
    (void)user_data;
    if (g_uppercasing) return;
    const char *text = gtk_editable_get_text(editable);
    char *upper = g_ascii_strup(text, -1);
    if (strcmp(upper, text) != 0) {
        g_uppercasing = TRUE;
        int pos = gtk_editable_get_position(editable);
        gtk_editable_set_text(editable, upper);
        gtk_editable_set_position(editable, pos);
        g_uppercasing = FALSE;
    }
    g_free(upper);
}

static void on_lookup_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    hx_perform_lookup(app, gtk_editable_get_text(GTK_EDITABLE(app->callsign_entry)));
}
static void on_callsign_activate(GtkEntry *e, gpointer user_data) {
    (void)e;
    on_lookup_clicked(NULL, user_data);
}

/* ============================== bookmarks / history ============================== */

static void on_bookmark_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    if (!app->current) return;
    hxdb_save_bookmark(app->db, app->current->callsign);
    hx_refresh_bookmarks(app);
    char *msg = g_strdup_printf("Bookmarked %s", app->current->callsign);
    hx_show_status(app, msg, 4000);
    g_free(msg);
}

static void on_bookmark_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    HxApp *app = user_data;
    GtkWidget *label = gtk_list_box_row_get_child(row);
    const char *cs = gtk_label_get_text(GTK_LABEL(label));
    gtk_editable_set_text(GTK_EDITABLE(app->callsign_entry), cs);
    hx_perform_lookup(app, cs);
}

static void on_history_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box;
    HxApp *app = user_data;
    GtkWidget *label = gtk_list_box_row_get_child(row);
    const char *text = gtk_label_get_text(GTK_LABEL(label));
    char cs[16] = {0};
    sscanf(text, "%15s", cs);
    gtk_editable_set_text(GTK_EDITABLE(app->callsign_entry), cs);
    hx_perform_lookup(app, cs);
}

static void remove_bookmark_response(GObject *src, GAsyncResult *res, gpointer user_data) {
    HxApp *app = user_data;
    GtkAlertDialog *alert = GTK_ALERT_DIALOG(src);
    GError *error = NULL;
    int idx = gtk_alert_dialog_choose_finish(alert, res, &error);
    if (!error && idx == 0) {
        GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->bookmarks_list));
        if (row) {
            GtkWidget *label = gtk_list_box_row_get_child(row);
            const char *cs = gtk_label_get_text(GTK_LABEL(label));
            char *cs_dup = g_strdup(cs);
            hxdb_remove_bookmark(app->db, cs_dup);
            hx_refresh_bookmarks(app);
            char *msg = g_strdup_printf("Removed bookmark: %s", cs_dup);
            hx_show_status(app, msg, 4000);
            g_free(msg);
            g_free(cs_dup);
        }
    }
    if (error) g_error_free(error);
}

static void on_remove_bookmark_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->bookmarks_list));
    if (!row) return;
    GtkWidget *label = gtk_list_box_row_get_child(row);
    const char *cs = gtk_label_get_text(GTK_LABEL(label));
    GtkAlertDialog *alert = gtk_alert_dialog_new("Remove %s from bookmarks?", cs);
    const char *buttons[] = {"Remove", "Cancel", NULL};
    gtk_alert_dialog_set_buttons(alert, buttons);
    gtk_alert_dialog_set_cancel_button(alert, 1);
    gtk_alert_dialog_choose(alert, app->window, NULL, remove_bookmark_response, app);
    g_object_unref(alert);
}

static void on_lookup_bookmark_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(app->bookmarks_list));
    if (row) on_bookmark_row_activated(GTK_LIST_BOX(app->bookmarks_list), row, app);
}

/* ============================== notes ============================== */

static void on_save_note_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    if (!app->current) return;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->notes_view));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    char *note = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    hxdb_save_note(app->db, app->current->callsign, note);
    char *msg = g_strdup_printf("Note %s for %s", (*g_strstrip(note)) ? "saved" : "deleted", app->current->callsign);
    hx_show_status(app, msg, 4000);
    g_free(msg);

    g_free(app->current->note);
    app->current->note = (*note) ? g_strdup(note) : NULL;
    hx_display_callsign(app, app->current);
    g_free(note);
}

/* ============================== action buttons ============================== */

static void on_map_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    if (!app->current) return;
    CallsignData *d = app->current;
    if (d->address && *d->address) {
        char *enc = url_encode(d->address);
        char *uri = g_strdup_printf("https://www.google.com/maps/search/?api=1&query=%s", enc);
        open_uri(app, uri);
        g_free(enc); g_free(uri);
    }
}
static void on_qrz_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    if (!app->current) return;
    char *uri = g_strdup_printf("https://www.qrz.com/db/%s", app->current->callsign);
    open_uri(app, uri);
    g_free(uri);
}
static void on_fcc_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    if (!app->current) return;
    if (app->current->fcc_url) {
        open_uri(app, app->current->fcc_url);
    } else {
        char *uri = g_strdup_printf(
            "https://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp?searchValue=%s",
            app->current->callsign);
        open_uri(app, uri);
        g_free(uri);
    }
}

static void export_save_cb(GObject *src, GAsyncResult *res, gpointer user_data) {
    HxApp *app = user_data;
    GtkFileDialog *dlg = GTK_FILE_DIALOG(src);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dlg, res, &error);
    if (!file) { if (error) g_error_free(error); return; }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    CallsignData *d = app->current;
    GError *werr = NULL;
    gboolean ok = FALSE;

    if (g_str_has_suffix(path, ".csv")) {
        GString *s = g_string_new("Field,Value\n");
        if (d->callsign) g_string_append_printf(s, "Callsign,%s\n", d->callsign);
        if (d->name) g_string_append_printf(s, "Name,%s\n", d->name);
        if (d->address) g_string_append_printf(s, "Address,\"%s\"\n", d->address);
        if (d->operator_class) g_string_append_printf(s, "Class,%s\n", d->operator_class);
        if (d->expiry_date) g_string_append_printf(s, "Expiry,%s\n", d->expiry_date);
        ok = g_file_set_contents(path, s->str, s->len, &werr);
        g_string_free(s, TRUE);
    } else if (g_str_has_suffix(path, ".txt")) {
        GString *s = g_string_new("");
        if (d->callsign) g_string_append_printf(s, "Callsign: %s\n", d->callsign);
        if (d->name) g_string_append_printf(s, "Name: %s\n", d->name);
        if (d->address) g_string_append_printf(s, "Address: %s\n", d->address);
        if (d->operator_class) g_string_append_printf(s, "Class: %s\n", d->operator_class);
        ok = g_file_set_contents(path, s->str, s->len, &werr);
        g_string_free(s, TRUE);
    } else {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        #define ADDSTR(key, val) do { json_builder_set_member_name(b, key); \
            if (val) json_builder_add_string_value(b, val); else json_builder_add_null_value(b); } while (0)
        ADDSTR("callsign", d->callsign);
        ADDSTR("name", d->name);
        ADDSTR("address", d->address);
        ADDSTR("operator_class", d->operator_class);
        ADDSTR("expiry_date", d->expiry_date);
        ADDSTR("license_status", d->license_status);
        ADDSTR("note", d->note);
        #undef ADDSTR
        json_builder_end_object(b);
        JsonGenerator *gen = json_generator_new();
        json_generator_set_pretty(gen, TRUE);
        json_generator_set_root(gen, json_builder_get_root(b));
        gsize len = 0;
        char *jsontext = json_generator_to_data(gen, &len);
        ok = g_file_set_contents(path, jsontext, len, &werr);
        g_free(jsontext);
        g_object_unref(gen);
        g_object_unref(b);
    }

    if (ok) {
        char *msg = g_strdup_printf("Exported to:\n%s", path);
        GtkAlertDialog *alert = gtk_alert_dialog_new("%s", msg);
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        g_free(msg);
    } else {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Export failed: %s", werr ? werr->message : "unknown error");
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        if (werr) g_error_free(werr);
    }
    g_free(path);
}

static void on_export_clicked(GtkButton *b, gpointer user_data) {
    (void)b;
    HxApp *app = user_data;
    if (!app->current) return;
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Export Callsign Data");
    char *suggested = g_strdup_printf("%s_data.json", app->current->callsign);
    gtk_file_dialog_set_initial_name(dlg, suggested);
    g_free(suggested);
    gtk_file_dialog_save(dlg, app->window, NULL, export_save_cb, app);
    g_object_unref(dlg);
}

/* ============================== settings widgets ============================== */

static void on_font_size_changed(GtkSpinButton *spin, gpointer user_data) {
    HxApp *app = user_data;
    app->font_size = gtk_spin_button_get_value_as_int(spin);
    /* hx_apply_theme() folds font_size into the same CSS provider as the
     * light/dark stylesheet, so it applies immediately here and (since
     * hx_load_settings() runs before hx_build_ui()) also on startup. */
    hx_apply_theme(app);
}

static void on_dark_switch_toggled(GtkSwitch *sw, GParamSpec *pspec, gpointer user_data) {
    (void)pspec;
    HxApp *app = user_data;
    app->dark_mode = gtk_switch_get_active(sw);
    hx_apply_theme(app);
}

static void on_grid_changed(GtkEditable *editable, gpointer user_data) {
    HxApp *app = user_data;
    g_free(app->home_grid);
    app->home_grid = g_strdup(gtk_editable_get_text(editable));
    if (app->current) hx_display_callsign(app, app->current);
}

/* ============================== menu actions ============================== */

static void action_toggle_theme(GSimpleAction *a, GVariant *p, gpointer user_data) {
    (void)a; (void)p;
    HxApp *app = user_data;
    app->dark_mode = !app->dark_mode;
    gtk_switch_set_active(GTK_SWITCH(app->dark_switch), app->dark_mode);
    hx_apply_theme(app);
}
static void action_clear_history(GSimpleAction *a, GVariant *p, gpointer user_data) {
    (void)a; (void)p;
    HxApp *app = user_data;
    hxdb_clear_history(app->db);
    hx_refresh_history(app);
    hx_show_status(app, "History cleared", 4000);
}
static void export_bookmarks_cb(GObject *src, GAsyncResult *res, gpointer user_data) {
    HxApp *app = user_data;
    GtkFileDialog *dlg = GTK_FILE_DIALOG(src);
    GError *error = NULL;
    GFile *file = gtk_file_dialog_save_finish(dlg, res, &error);
    if (!file) { if (error) g_error_free(error); return; }
    char *path = g_file_get_path(file);
    g_object_unref(file);
    GPtrArray *bms = hxdb_get_bookmarks(app->db);
    gboolean ok;
    GError *werr = NULL;
    if (g_str_has_suffix(path, ".csv")) {
        GString *s = g_string_new("Callsign\n");
        for (guint i = 0; i < bms->len; i++)
            g_string_append_printf(s, "%s\n", (const char *)g_ptr_array_index(bms, i));
        ok = g_file_set_contents(path, s->str, s->len, &werr);
        g_string_free(s, TRUE);
    } else {
        JsonBuilder *b = json_builder_new();
        json_builder_begin_object(b);
        json_builder_set_member_name(b, "bookmarks");
        json_builder_begin_array(b);
        for (guint i = 0; i < bms->len; i++)
            json_builder_add_string_value(b, g_ptr_array_index(bms, i));
        json_builder_end_array(b);
        json_builder_end_object(b);
        JsonGenerator *gen = json_generator_new();
        json_generator_set_pretty(gen, TRUE);
        json_generator_set_root(gen, json_builder_get_root(b));
        gsize len = 0;
        char *text = json_generator_to_data(gen, &len);
        ok = g_file_set_contents(path, text, len, &werr);
        g_free(text);
        g_object_unref(gen);
        g_object_unref(b);
    }
    if (ok) {
        char *msg = g_strdup_printf("Exported %u bookmarks to:\n%s", bms->len, path);
        GtkAlertDialog *alert = gtk_alert_dialog_new("%s", msg);
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        g_free(msg);
    } else {
        GtkAlertDialog *alert = gtk_alert_dialog_new("Export failed: %s", werr ? werr->message : "unknown");
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        if (werr) g_error_free(werr);
    }
    g_ptr_array_free(bms, TRUE);
    g_free(path);
}
static void action_export_bookmarks(GSimpleAction *a, GVariant *p, gpointer user_data) {
    (void)a; (void)p;
    HxApp *app = user_data;
    GPtrArray *bms = hxdb_get_bookmarks(app->db);
    if (bms->len == 0) {
        GtkAlertDialog *alert = gtk_alert_dialog_new("You have no bookmarks to export.");
        gtk_alert_dialog_show(alert, app->window);
        g_object_unref(alert);
        g_ptr_array_free(bms, TRUE);
        return;
    }
    g_ptr_array_free(bms, TRUE);
    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Export Bookmarks");
    gtk_file_dialog_set_initial_name(dlg, "hamdex_bookmarks.json");
    gtk_file_dialog_save(dlg, app->window, NULL, export_bookmarks_cb, app);
    g_object_unref(dlg);
}
static void action_about(GSimpleAction *a, GVariant *p, gpointer user_data) {
    (void)a; (void)p;
    HxApp *app = user_data;
    GtkAlertDialog *alert = gtk_alert_dialog_new(
        "HamDex " HAMDEX_VERSION "\n\n"
        "Amateur Radio Callsign Lookup Tool\n"
        "Lookup source: Local FCC ULS database\n\n"
        "FCC ULS local database  ·  Grid Calculator  ·  Bookmarks & Notes\n\n"
        "Built with GTK4 · SQLite · libcurl · C11");
    gtk_alert_dialog_show(alert, app->window);
    g_object_unref(alert);
}

/* ============================== keyboard shortcuts ============================== */

static gboolean on_key_pressed(GtkEventControllerKey *ctrl, guint keyval, guint keycode,
                                GdkModifierType state, gpointer user_data) {
    (void)ctrl; (void)keycode;
    HxApp *app = user_data;
    if (keyval == GDK_KEY_F5) {
        on_lookup_clicked(NULL, app);
        return TRUE;
    }
    if (keyval == GDK_KEY_Escape) {
        gtk_editable_set_text(GTK_EDITABLE(app->callsign_entry), "");
        return TRUE;
    }
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_b || keyval == GDK_KEY_B)) {
        on_bookmark_clicked(NULL, app);
        return TRUE;
    }
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_l || keyval == GDK_KEY_L)) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(app->main_notebook), 0);
        gtk_widget_grab_focus(app->callsign_entry);
        return TRUE;
    }
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_t || keyval == GDK_KEY_T)) {
        action_toggle_theme(NULL, NULL, app);
        return TRUE;
    }
    if ((state & GDK_CONTROL_MASK) && (keyval == GDK_KEY_q || keyval == GDK_KEY_Q)) {
        gtk_window_close(app->window);
        return TRUE;
    }
    return FALSE;
}

/* ============================== layout builders ============================== */

/* expand=TRUE lets the field's column grow to fill leftover horizontal
 * space in its row (used for most filter fields); pass FALSE for fields
 * that should stay at their natural/fixed width (e.g. the 2-letter State
 * box). */
static GtkWidget *labeled_ex(const char *text, GtkWidget *widget, gboolean expand) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_add_css_class(lbl, "dim");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_box_append(GTK_BOX(box), widget);
    if (expand) {
        gtk_widget_set_hexpand(widget, TRUE);
        gtk_widget_set_hexpand(box, TRUE);
    }
    return box;
}

static GtkWidget *labeled(const char *text, GtkWidget *widget) {
    return labeled_ex(text, widget, TRUE);
}

static GtkWidget *build_left_panel(HxApp *app) {
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(panel, "leftpanel");
    gtk_widget_set_size_request(panel, 340, -1);
    gtk_widget_set_margin_start(panel, 8); gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 8); gtk_widget_set_margin_bottom(panel, 8);

    /* Lookup group */
    GtkWidget *lookup_frame = gtk_frame_new("Callsign Lookup");
    GtkWidget *lookup_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(lookup_box, 10); gtk_widget_set_margin_end(lookup_box, 10);
    gtk_widget_set_margin_top(lookup_box, 10); gtk_widget_set_margin_bottom(lookup_box, 10);
    app->callsign_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->callsign_entry), "Enter callsign (e.g., W1AW)  |  F5 to search");
    g_signal_connect(app->callsign_entry, "changed", G_CALLBACK(on_callsign_changed), app);
    g_signal_connect(app->callsign_entry, "activate", G_CALLBACK(on_callsign_activate), app);
    gtk_box_append(GTK_BOX(lookup_box), app->callsign_entry);
    app->lookup_btn = gtk_button_new_with_label("Lookup Callsign  [F5]");
    gtk_widget_add_css_class(app->lookup_btn, "primary");
    g_signal_connect(app->lookup_btn, "clicked", G_CALLBACK(on_lookup_clicked), app);
    gtk_box_append(GTK_BOX(lookup_box), app->lookup_btn);
    app->bookmark_btn = gtk_button_new_with_label("Add to Bookmarks  [Ctrl+B]");
    gtk_widget_set_sensitive(app->bookmark_btn, FALSE);
    g_signal_connect(app->bookmark_btn, "clicked", G_CALLBACK(on_bookmark_clicked), app);
    gtk_box_append(GTK_BOX(lookup_box), app->bookmark_btn);
    gtk_frame_set_child(GTK_FRAME(lookup_frame), lookup_box);
    gtk_box_append(GTK_BOX(panel), lookup_frame);

    /* Bookmarks group */
    GtkWidget *bm_frame = gtk_frame_new("Bookmarks");
    GtkWidget *bm_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(bm_box, 10); gtk_widget_set_margin_end(bm_box, 10);
    gtk_widget_set_margin_top(bm_box, 10); gtk_widget_set_margin_bottom(bm_box, 10);
    app->bookmarks_list = gtk_list_box_new();
    GtkWidget *bm_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(bm_scroll), app->bookmarks_list);
    gtk_widget_set_size_request(bm_scroll, -1, 120);
    g_signal_connect(app->bookmarks_list, "row-activated", G_CALLBACK(on_bookmark_row_activated), app);
    gtk_box_append(GTK_BOX(bm_box), bm_scroll);
    GtkWidget *bm_btns = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *lu_bk = gtk_button_new_with_label("Lookup");
    g_signal_connect(lu_bk, "clicked", G_CALLBACK(on_lookup_bookmark_clicked), app);
    GtkWidget *rm_bk = gtk_button_new_with_label("Remove");
    gtk_widget_add_css_class(rm_bk, "accent");
    g_signal_connect(rm_bk, "clicked", G_CALLBACK(on_remove_bookmark_clicked), app);
    gtk_box_append(GTK_BOX(bm_btns), lu_bk);
    gtk_box_append(GTK_BOX(bm_btns), rm_bk);
    gtk_box_append(GTK_BOX(bm_box), bm_btns);
    gtk_frame_set_child(GTK_FRAME(bm_frame), bm_box);
    gtk_box_append(GTK_BOX(panel), bm_frame);

    /* History group */
    GtkWidget *h_frame = gtk_frame_new("Recent History");
    GtkWidget *h_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(h_box, 10); gtk_widget_set_margin_end(h_box, 10);
    gtk_widget_set_margin_top(h_box, 10); gtk_widget_set_margin_bottom(h_box, 10);
    app->history_list = gtk_list_box_new();
    GtkWidget *h_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(h_scroll), app->history_list);
    gtk_widget_set_size_request(h_scroll, -1, 150);
    g_signal_connect(app->history_list, "row-activated", G_CALLBACK(on_history_row_activated), app);
    gtk_box_append(GTK_BOX(h_box), h_scroll);
    gtk_frame_set_child(GTK_FRAME(h_frame), h_box);
    gtk_box_append(GTK_BOX(panel), h_frame);

    /* Settings group */
    GtkWidget *s_frame = gtk_frame_new("Settings");
    GtkWidget *s_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(s_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(s_grid), 8);
    gtk_widget_set_margin_start(s_grid, 10); gtk_widget_set_margin_end(s_grid, 10);
    gtk_widget_set_margin_top(s_grid, 10); gtk_widget_set_margin_bottom(s_grid, 10);

    GtkWidget *fs_lbl = gtk_label_new("Font Size:");
    gtk_label_set_xalign(GTK_LABEL(fs_lbl), 0.0);
    app->font_spin = gtk_spin_button_new_with_range(8, 20, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->font_spin), app->font_size);
    g_signal_connect(app->font_spin, "value-changed", G_CALLBACK(on_font_size_changed), app);
    gtk_grid_attach(GTK_GRID(s_grid), fs_lbl, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(s_grid), app->font_spin, 1, 0, 1, 1);

    GtkWidget *dm_lbl = gtk_label_new("Dark Mode:");
    gtk_label_set_xalign(GTK_LABEL(dm_lbl), 0.0);
    app->dark_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(app->dark_switch), app->dark_mode);
    gtk_widget_set_halign(app->dark_switch, GTK_ALIGN_START);
    g_signal_connect(app->dark_switch, "notify::active", G_CALLBACK(on_dark_switch_toggled), app);
    gtk_grid_attach(GTK_GRID(s_grid), dm_lbl, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(s_grid), app->dark_switch, 1, 1, 1, 1);

    GtkWidget *hg_lbl = gtk_label_new("Home Grid:");
    gtk_label_set_xalign(GTK_LABEL(hg_lbl), 0.0);
    app->grid_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->grid_entry), "e.g. DM79");
    gtk_editable_set_text(GTK_EDITABLE(app->grid_entry), app->home_grid);
    gtk_widget_set_tooltip_text(app->grid_entry, "Your Maidenhead grid square for distance/bearing calculations.");
    g_signal_connect(app->grid_entry, "changed", G_CALLBACK(on_grid_changed), app);
    gtk_grid_attach(GTK_GRID(s_grid), hg_lbl, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(s_grid), app->grid_entry, 1, 2, 1, 1);

    gtk_frame_set_child(GTK_FRAME(s_frame), s_grid);
    gtk_box_append(GTK_BOX(panel), s_frame);

    return panel;
}

static GtkWidget *build_details_panel(HxApp *app) {
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(panel, "detailspanel");
    gtk_widget_set_margin_start(panel, 8); gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 8); gtk_widget_set_margin_bottom(panel, 8);
    gtk_widget_set_hexpand(panel, TRUE);

    app->callsign_label = gtk_label_new("No Data");
    gtk_widget_add_css_class(app->callsign_label, "callsign");
    gtk_widget_set_margin_top(app->callsign_label, 10);
    gtk_box_append(GTK_BOX(panel), app->callsign_label);

    app->operator_label = gtk_label_new("");
    gtk_widget_add_css_class(app->operator_label, "dim");
    gtk_box_append(GTK_BOX(panel), app->operator_label);

    app->status_badge = gtk_label_new("");
    gtk_widget_set_visible(app->status_badge, FALSE);
    gtk_widget_set_halign(app->status_badge, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(app->status_badge, 8);
    gtk_box_append(GTK_BOX(panel), app->status_badge);

    app->details_notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(app->details_notebook, TRUE);

    /* Details tab */
    app->info_cards_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(app->info_cards_box, 6); gtk_widget_set_margin_end(app->info_cards_box, 6);
    gtk_widget_set_margin_top(app->info_cards_box, 6);
    GtkWidget *details_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(details_scroll), app->info_cards_box);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->details_notebook), details_scroll, gtk_label_new("Details"));

    /* Notes tab */
    GtkWidget *notes_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(notes_box, 10); gtk_widget_set_margin_end(notes_box, 10);
    gtk_widget_set_margin_top(notes_box, 10); gtk_widget_set_margin_bottom(notes_box, 10);
    GtkWidget *notes_title = gtk_label_new("Personal Notes");
    gtk_widget_set_halign(notes_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(notes_box), notes_title);
    app->notes_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->notes_view), GTK_WRAP_WORD);
    GtkWidget *notes_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(notes_scroll), app->notes_view);
    gtk_widget_set_vexpand(notes_scroll, TRUE);
    gtk_box_append(GTK_BOX(notes_box), notes_scroll);
    GtkWidget *save_note_btn = gtk_button_new_with_label("Save Note");
    gtk_widget_add_css_class(save_note_btn, "primary");
    gtk_widget_set_halign(save_note_btn, GTK_ALIGN_END);
    g_signal_connect(save_note_btn, "clicked", G_CALLBACK(on_save_note_clicked), app);
    gtk_box_append(GTK_BOX(notes_box), save_note_btn);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->details_notebook), notes_box, gtk_label_new("Notes"));

    gtk_box_append(GTK_BOX(panel), app->details_notebook);

    /* Action buttons row */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    app->map_btn = gtk_button_new_with_label("Map");
    app->qrz_btn = gtk_button_new_with_label("QRZ.com");
    app->fcc_btn = gtk_button_new_with_label("FCC ULS");
    app->export_btn = gtk_button_new_with_label("Export");
    gtk_widget_add_css_class(app->export_btn, "accent");
    gtk_widget_set_sensitive(app->map_btn, FALSE);
    gtk_widget_set_sensitive(app->qrz_btn, FALSE);
    gtk_widget_set_sensitive(app->fcc_btn, FALSE);
    gtk_widget_set_sensitive(app->export_btn, FALSE);
    g_signal_connect(app->map_btn, "clicked", G_CALLBACK(on_map_clicked), app);
    g_signal_connect(app->qrz_btn, "clicked", G_CALLBACK(on_qrz_clicked), app);
    g_signal_connect(app->fcc_btn, "clicked", G_CALLBACK(on_fcc_clicked), app);
    g_signal_connect(app->export_btn, "clicked", G_CALLBACK(on_export_clicked), app);
    gtk_box_append(GTK_BOX(btn_row), app->map_btn);
    gtk_box_append(GTK_BOX(btn_row), app->qrz_btn);
    gtk_box_append(GTK_BOX(btn_row), app->fcc_btn);
    gtk_box_append(GTK_BOX(btn_row), app->export_btn);
    gtk_box_append(GTK_BOX(panel), btn_row);

    return panel;
}

static GtkTreeView *build_results_tree(HxApp *app) {
    const char *cols[] = {"Callsign","Name","Entity","Address","City","State","ZIP",
                           "Class","Group","Status","Grant Date","Effective Date",
                           "Expiry Date","Cancelled","Trustee","Prev. Call"};
    GType types[16];
    for (int i = 0; i < 16; i++) types[i] = G_TYPE_STRING;
    app->results_store = gtk_list_store_newv(16, types);
    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->results_store));
    for (int i = 0; i < 16; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(cols[i], r, "text", i, NULL);
        gtk_tree_view_column_set_resizable(col, TRUE);
        gtk_tree_view_column_set_sort_column_id(col, i);
        gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    }
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), 0);
    app->results_view = view;
    return GTK_TREE_VIEW(view);
}

static GtkWidget *build_fcc_tab(HxApp *app) {
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(panel, "detailspanel");
    gtk_widget_set_margin_start(panel, 8); gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 8); gtk_widget_set_margin_bottom(panel, 8);

    /* Status + download buttons */
    GtkWidget *status_frame = gtk_frame_new("FCC ULS Database");
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(status_box, 10); gtk_widget_set_margin_end(status_box, 10);
    gtk_widget_set_margin_top(status_box, 10); gtk_widget_set_margin_bottom(status_box, 10);
    app->fcc_status_lbl = gtk_label_new("No database loaded");
    gtk_widget_add_css_class(app->fcc_status_lbl, "dim");
    gtk_widget_set_hexpand(app->fcc_status_lbl, TRUE);
    gtk_label_set_xalign(GTK_LABEL(app->fcc_status_lbl), 0.0);
    gtk_box_append(GTK_BOX(status_box), app->fcc_status_lbl);
    app->fcc_daily_btn = gtk_button_new_with_label("Update (Daily)");
    app->fcc_weekly_btn = gtk_button_new_with_label("Full Weekly");
    gtk_widget_add_css_class(app->fcc_weekly_btn, "primary");
    app->fcc_local_btn = gtk_button_new_with_label("Load Local ZIP");
    gtk_box_append(GTK_BOX(status_box), app->fcc_daily_btn);
    gtk_box_append(GTK_BOX(status_box), app->fcc_weekly_btn);
    gtk_box_append(GTK_BOX(status_box), app->fcc_local_btn);
    gtk_frame_set_child(GTK_FRAME(status_frame), status_box);
    gtk_box_append(GTK_BOX(panel), status_frame);

    /* Search filters */
    GtkWidget *filt_frame = gtk_frame_new("Search Filters");
    GtkWidget *filt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(filt_box, 10); gtk_widget_set_margin_end(filt_box, 10);
    gtk_widget_set_margin_top(filt_box, 10); gtk_widget_set_margin_bottom(filt_box, 10);

    app->f_name = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->f_name), "Smith");
    app->f_call = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->f_call), "W1AW or W1*");
    const char *classes[] = {"Any","Novice","Tech","General","Advanced","Extra", NULL};
    app->f_class = gtk_drop_down_new_from_strings(classes);
    const char *statuses[] = {"Any","Active","Expired","Cancelled", NULL};
    app->f_status = gtk_drop_down_new_from_strings(statuses);

    GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(row1), labeled("Name", app->f_name));
    gtk_box_append(GTK_BOX(row1), labeled("Callsign", app->f_call));
    gtk_box_append(GTK_BOX(row1), labeled("Class", app->f_class));
    gtk_box_append(GTK_BOX(row1), labeled("Status", app->f_status));
    gtk_box_append(GTK_BOX(filt_box), row1);

    app->f_address = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->f_address), "123 Main St");
    app->f_city = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->f_city), "Denver");
    app->f_state = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->f_state), "CO");
    gtk_widget_set_size_request(app->f_state, 60, -1);

    GtkWidget *row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(row2), labeled("Address", app->f_address));
    gtk_box_append(GTK_BOX(row2), labeled("City", app->f_city));
    gtk_box_append(GTK_BOX(row2), labeled_ex("State", app->f_state, FALSE));
    GtkWidget *search_btn = gtk_button_new_with_label("Search");
    gtk_widget_add_css_class(search_btn, "primary");
    GtkWidget *clear_btn = gtk_button_new_with_label("Clear");
    gtk_box_append(GTK_BOX(row2), search_btn);
    gtk_box_append(GTK_BOX(row2), clear_btn);
    gtk_box_append(GTK_BOX(filt_box), row2);

    gtk_frame_set_child(GTK_FRAME(filt_frame), filt_box);
    gtk_box_append(GTK_BOX(panel), filt_frame);

    app->result_lbl = gtk_label_new("Results appear here after a search.");
    gtk_widget_add_css_class(app->result_lbl, "dim");
    gtk_label_set_xalign(GTK_LABEL(app->result_lbl), 0.0);
    gtk_box_append(GTK_BOX(panel), app->result_lbl);

    GtkTreeView *tv = build_results_tree(app);
    gtk_tree_view_set_headers_clickable(tv, TRUE);
    GtkWidget *tv_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tv_scroll), GTK_WIDGET(tv));
    gtk_widget_set_vexpand(tv_scroll, TRUE);
    gtk_widget_set_size_request(tv_scroll, -1, 300);
    gtk_box_append(GTK_BOX(panel), tv_scroll);

    GtkWidget *hint = gtk_label_new("Double-click a row to look up that callsign on the Lookup tab.");
    gtk_widget_add_css_class(hint, "dim");
    gtk_box_append(GTK_BOX(panel), hint);

    hx_fcc_tab_connect(app);
    hx_fcc_search_connect(app, search_btn, clear_btn);

    return panel;
}

/* ============================== menu / header bar ============================== */

static void build_header_bar(HxApp *app) {
    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(app->window, header);

    GMenu *menu = g_menu_new();
    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "Export All Bookmarks...", "app.export_bookmarks");
    g_menu_append_submenu(menu, "File", G_MENU_MODEL(file_menu));
    GMenu *tools_menu = g_menu_new();
    g_menu_append(tools_menu, "Clear History", "app.clear_history");
    g_menu_append_submenu(menu, "Tools", G_MENU_MODEL(tools_menu));
    GMenu *view_menu = g_menu_new();
    g_menu_append(view_menu, "Toggle Theme (Ctrl+T)", "app.toggle_theme");
    g_menu_append_submenu(menu, "View", G_MENU_MODEL(view_menu));
    GMenu *help_menu = g_menu_new();
    g_menu_append(help_menu, "About HamDex", "app.about");
    g_menu_append_submenu(menu, "Help", G_MENU_MODEL(help_menu));

    GtkWidget *menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), G_MENU_MODEL(menu));
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_btn);
    g_object_unref(menu);

    static const GActionEntry entries[] = {
        { "export_bookmarks", action_export_bookmarks, NULL, NULL, NULL },
        { "clear_history", action_clear_history, NULL, NULL, NULL },
        { "toggle_theme", action_toggle_theme, NULL, NULL, NULL },
        { "about", action_about, NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app->gapp), entries, G_N_ELEMENTS(entries), app);
}

/* ============================== top-level build ============================== */

void hx_build_ui(HxApp *app) {
    app->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
        GTK_STYLE_PROVIDER(app->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_window_set_title(app->window, "HamDex - Amateur Radio Callsign Lookup");
    gtk_window_set_default_size(app->window, 1100, 700);
    build_header_bar(app);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(app->window, root);

    app->main_notebook = gtk_notebook_new();
    gtk_widget_set_vexpand(app->main_notebook, TRUE);
    gtk_box_append(GTK_BOX(root), app->main_notebook);

    GtkWidget *lookup_page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *left = build_left_panel(app);
    GtkWidget *right = build_details_panel(app);
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_start_child(GTK_PANED(paned), left);
    gtk_paned_set_end_child(GTK_PANED(paned), right);
    gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
    gtk_paned_set_position(GTK_PANED(paned), 360);
    gtk_widget_set_hexpand(paned, TRUE);
    gtk_box_append(GTK_BOX(lookup_page), paned);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->main_notebook), lookup_page, gtk_label_new("Callsign Lookup"));

    GtkWidget *fcc_page = build_fcc_tab(app);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->main_notebook), fcc_page, gtk_label_new("FCC Database"));

    app->statusbar = gtk_label_new("");
    gtk_widget_add_css_class(app->statusbar, "dim");
    gtk_label_set_xalign(GTK_LABEL(app->statusbar), 0.0);
    gtk_widget_set_margin_start(app->statusbar, 10);
    gtk_widget_set_margin_end(app->statusbar, 10);
    gtk_widget_set_margin_top(app->statusbar, 4);
    gtk_widget_set_margin_bottom(app->statusbar, 4);
    gtk_box_append(GTK_BOX(root), app->statusbar);

    hx_apply_theme(app);

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_key_pressed), app);
    gtk_widget_add_controller(GTK_WIDGET(app->window), key_ctrl);

    hx_refresh_bookmarks(app);
    hx_refresh_history(app);
    hx_clear_details(app);
}
