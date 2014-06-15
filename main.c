#include <stdlib.h>
#include <gtk/gtk.h>

#include "libfft/fft-pgm.h"
#include "spektro-audio.h"

static int open_file(char *file_name, GtkImage *canvas) {
  int tmp_file;
  char tmp_audio_fname[] = "/tmp/spektro_XXXXXX.wav";
  char tmp_image_fname[] = "/tmp/spektro_XXXXXX.pgm";

  // create tempfile for f32le pcm
  tmp_file = mkstemps(tmp_audio_fname, 4);
  if (tmp_file == -1) {
    g_warning("open_file(): mkstemps() failed");
    return -1;
  }
  close(tmp_file);

  // create tempfile for image
  tmp_file = mkstemps(tmp_image_fname, 4);
  if (tmp_file == -1) {
    g_warning("open_file(): mkstemps() failed");
    return -1;
  }

  // TODO error checking for extract_raw_audio()
  extract_raw_audio(file_name, tmp_audio_fname);
  create_rdft_image(10.0f, 10, tmp_audio_fname, tmp_file);

  gtk_image_set_from_file(canvas, tmp_image_fname);
  return 0;
}

static void open_cb(GtkMenuItem *menuitem, GtkBuilder *builder) {
  GtkFileChooserDialog *file_chooser = GTK_FILE_CHOOSER_DIALOG(gtk_builder_get_object(builder, "file-chooser-dialog"));
  gint result = gtk_dialog_run(GTK_DIALOG(file_chooser));
  g_message("open_cb result: %d, accept: %d\n", result, GTK_RESPONSE_ACCEPT);
  switch (result) {
    case GTK_RESPONSE_APPLY:
    {
      g_message("open file");
      char *file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser));
      GtkImage *canvas = GTK_IMAGE(gtk_builder_get_object(builder, "canvas"));
      open_file(file_name, canvas);
      g_free(file_name);
      break;
    }
    case GTK_RESPONSE_CANCEL:
      // fallthrough
    default:
      break;
  }
  gtk_widget_hide(GTK_WIDGET(file_chooser));
}

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
      g_message("apply preferences");
      break;
    case GTK_RESPONSE_CANCEL:
      // fallthrough
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
  GtkFileChooserDialog *file_chooser = GTK_FILE_CHOOSER_DIALOG(gtk_builder_get_object(builder, "file-chooser-dialog"));
  GtkMenuItem *menu_open = GTK_MENU_ITEM(gtk_builder_get_object(builder, "menu-file-open"));
  g_signal_connect(G_OBJECT(menu_open), "activate", G_CALLBACK(open_cb), builder);

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
