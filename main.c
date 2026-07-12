/* main.c — HamDex entry point: GtkApplication lifecycle glue */
#include "hamdex_app.h"
#include "app.h"

static void on_activate(GApplication *gapp, gpointer user_data) {
    HxApp *app = user_data;

    /* Re-activation (e.g. second launch attempt) should just re-present
     * the existing window rather than rebuilding everything. */
    if (app->window) {
        gtk_window_present(app->window);
        return;
    }

    app->window = GTK_WINDOW(gtk_application_window_new(GTK_APPLICATION(gapp)));

    /* Open databases before hx_build_ui(), since it immediately calls
     * hx_refresh_bookmarks()/hx_refresh_history() which need app->db,
     * and the FCC tab needs app->fcc_db for its status label. */
    app->db = hxdb_open(NULL);
    app->fcc_db = fccdb_open(NULL);

    /* Load dark_mode/font_size/home_grid before hx_build_ui(), since
     * hx_apply_theme() (called from inside hx_build_ui) depends on them. */
    hx_load_settings(app);

    hx_build_ui(app);

    gtk_window_present(app->window);
}

static void on_shutdown(GApplication *gapp, gpointer user_data) {
    (void)gapp;
    HxApp *app = user_data;
    hx_save_settings(app);
    if (app->fcc_db) fccdb_close(app->fcc_db);
    if (app->db) hxdb_close(app->db);
}

int main(int argc, char *argv[]) {
    HxApp app_data = {0};

    app_data.gapp = gtk_application_new("net.n5rr.hamdex", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app_data.gapp, "activate", G_CALLBACK(on_activate), &app_data);
    g_signal_connect(app_data.gapp, "shutdown", G_CALLBACK(on_shutdown), &app_data);

    int status = g_application_run(G_APPLICATION(app_data.gapp), argc, argv);

    g_object_unref(app_data.gapp);
    return status;
}
