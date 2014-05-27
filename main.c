#include <gtk/gtk.h>

static void quit_cb(GtkMenuItem *menuitem, GtkApplication *app) {
  GList *window_list = gtk_application_get_windows(app);
  for (GList *w = window_list; w != NULL; w = w->next) {
    gtk_application_remove_window(app, GTK_WINDOW(w->data));
  }
}
static void about_cb(GtkMenuItem *menuitem, GtkAboutDialog *about) {
  gint result = gtk_dialog_run(GTK_DIALOG(about));
  gtk_widget_hide(GTK_WIDGET(about));
}
static void prefs_cb(GtkMenuItem *menuitem, GtkDialog *prefs) {
  gint result = gtk_dialog_run(GTK_DIALOG(prefs));
  switch (result) {
    case GTK_RESPONSE_APPLY:
      // TODO
      g_debug("apply preferences");
      break;
    case GTK_RESPONSE_CANCEL:
      g_debug("cancel");
      break;
    default:
      break;
  }
  gtk_widget_hide(GTK_WIDGET(prefs));
}

static void spektro_startup_cb(GtkApplication *app, gpointer user_data) {
  gtk_application_set_menubar(GTK_APPLICATION(app), NULL);
  gtk_application_set_app_menu(GTK_APPLICATION(app), NULL);
  return;
}
static void spektro_activate_cb(GtkApplication *app, gpointer user_data) {
  GtkBuilder *builder = gtk_builder_new_from_file("spektro.ui");

  GtkApplicationWindow *app_window = GTK_APPLICATION_WINDOW(
      gtk_builder_get_object(builder, "main-window"));

  g_object_set(G_OBJECT(app_window), "application", app, NULL);
  gtk_application_window_set_show_menubar(app_window, FALSE);

  GtkGrid *grid = GTK_GRID(gtk_builder_get_object(builder, "grid"));

  // set up area where the spectrograph is displayed
  GtkScrolledWindow *scroll =GTK_SCROLLED_WINDOW(
      gtk_builder_get_object(builder, "scrolledwindow"));

  GtkImage *canvas = GTK_IMAGE(gtk_builder_get_object(builder, "canvas"));

  // file menu
  GtkMenuItem *menu_quit = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menu-file-quit"));
  g_signal_connect(G_OBJECT(menu_quit), "activate", G_CALLBACK(quit_cb), app);

  // open preferences window with menu
  GtkDialog *prefs = GTK_DIALOG(gtk_builder_get_object(builder, "preferences-dialog"));
  GtkMenuItem *menu_prefs = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menu-edit-prefs"));
  g_signal_connect(G_OBJECT(menu_prefs), "activate", G_CALLBACK(prefs_cb), prefs);

  // about dialog
  GtkAboutDialog *about = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about-dialog"));
  GtkMenuItem *menu_about = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menu-help-about"));
  g_signal_connect(G_OBJECT(menu_about), "activate", G_CALLBACK(about_cb), about);

  //gtk_widget_show(GTK_WIDGET(app_window));
  return;
}

int main(int argc, char *argv[]) {
  gint status;
  GtkApplication *spektro_app = gtk_application_new("org.spektro",
      G_APPLICATION_NON_UNIQUE | G_APPLICATION_HANDLES_OPEN);

  g_signal_connect(G_OBJECT(spektro_app), "startup",
      G_CALLBACK(spektro_startup_cb), NULL);
  g_signal_connect(G_OBJECT(spektro_app), "activate",
      G_CALLBACK(spektro_activate_cb), NULL);

  if (!g_application_register(G_APPLICATION(spektro_app), NULL, NULL)) {
    g_error("application failed to register");
  }

  status = g_application_run(G_APPLICATION(spektro_app), argc, argv);
  g_object_unref(spektro_app);

  return status;
}
