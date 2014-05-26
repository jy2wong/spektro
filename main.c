#include <gtk/gtk.h>

static gboolean canvas_draw_cb(GtkWidget *widget, cairo_t *cr, gpointer data) {
  guint width, height;
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);
  cairo_arc (cr,
                   width / 2.0, height / 2.0,
                                MIN (width, height) / 2.0,
                                             0, 2 * G_PI);
  return FALSE;
}
static void spektro_startup_cb(GtkApplication *app, gpointer user_data) {
  gtk_application_set_menubar(GTK_APPLICATION(app), NULL);
  gtk_application_set_app_menu(GTK_APPLICATION(app), NULL);
  return;
}
static void spektro_activate_cb(GtkApplication *app, gpointer user_data) {
  GtkBuilder *builder = gtk_builder_new_from_file("spektro.ui");

  GtkApplicationWindow *app_window = GTK_APPLICATION_WINDOW(gtk_builder_get_object(builder, "window"));
  g_object_set(G_OBJECT(app_window), "application", app, NULL);
  gtk_application_window_set_show_menubar(app_window, FALSE);

  GtkGrid *grid = GTK_GRID(gtk_builder_get_object(builder, "grid"));

  // set up area where the spectrograph is displayed
  GtkScrolledWindow *scroll = GTK_SCROLLED_WINDOW(gtk_builder_get_object(builder, "scrolledwindow"));
  GtkDrawingArea *canvas = GTK_DRAWING_AREA(gtk_builder_get_object(builder, "canvas"));
  g_signal_connect(G_OBJECT(canvas), "draw", G_CALLBACK(canvas_draw_cb), NULL);

  //GtkLabel *label = GTK_LABEL(gtk_label_new("asdf"));
  //gtk_grid_attach(grid, GTK_WIDGET(label), 0, 0, 1, 1);
  //gtk_widget_show(GTK_WIDGET(label));

  //gtk_widget_show(GTK_WIDGET(grid));
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

  g_application_register(G_APPLICATION(spektro_app), NULL, NULL);

  status = g_application_run(G_APPLICATION(spektro_app), argc, argv);
  //g_object_unref(spektro_app);

  return status;
}
