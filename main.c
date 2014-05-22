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
  GtkBuilder *builder = gtk_builder_new_from_string(
"<interface>"
"<object id='window' class='GtkApplicationWindow'>"
"  <property name='title'>Spektro</property>"
"  <property name='visible'>True</property>"
"  <child>"
"    <object id='grid' class='GtkGrid'>"
"      <property name='visible'>True</property>"
"      <property name='can_focus'>False</property>"
"      <child>"
"        <object class='GtkGrid' id='grid3'>"
"          <property name='visible'>True</property>"
"          <property name='can_focus'>False</property>"
"          <child>"
"            <object class='GtkLabel' id='lower-label'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <property name='halign'>end</property>"
"              <property name='label' translatable='yes'>Lower:</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>0</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkSpinButton' id='lower-spin'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>True</property>"
"              <property name='max_length'>7</property>"
"              <property name='width_chars'>7</property>"
"              <property name='max_width_chars'>7</property>"
"              <property name='input_purpose'>number</property>"
"              <property name='orientation'>vertical</property>"
"              <property name='digits'>1</property>"
"              <property name='numeric'>True</property>"
"              <property name='update_policy'>if-valid</property>"
"              <property name='value'>20</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>1</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkLabel' id='lower-label-unit'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <property name='halign'>start</property>"
"              <property name='margin_right'>16</property>"
"              <property name='label' translatable='yes'>Hz</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>2</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkLabel' id='upper-label'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <property name='halign'>end</property>"
"              <property name='label' translatable='yes'>Upper:</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>3</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkSpinButton' id='upper-spin'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>True</property>"
"              <property name='max_width_chars'>7</property>"
"              <property name='orientation'>vertical</property>"
"              <property name='digits'>1</property>"
"              <property name='numeric'>True</property>"
"              <property name='update_policy'>if-valid</property>"
"              <property name='value'>20000</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>4</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkLabel' id='lower-label-unit1'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <property name='halign'>start</property>"
"              <property name='margin_right'>16</property>"
"              <property name='label' translatable='yes'>Hz</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>5</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkLabel' id='scaling-label'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <property name='halign'>end</property>"
"              <property name='label' translatable='yes'>Scaling:</property>"
"              <property name='track_visited_links'>False</property>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>6</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"          <child>"
"            <object class='GtkComboBoxText' id='scaling-combobox'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <property name='valign'>center</property>"
"              <items>"
"                <item id='scaling-label-logarithmic' translatable='yes'>Logarithmic</item>"
"                <item id='scaling-label-linear' translatable='yes'>Linear</item>"
"              </items>"
"            </object>"
"            <packing>"
"              <property name='left_attach'>7</property>"
"              <property name='top_attach'>0</property>"
"            </packing>"
"          </child>"
"        </object>"
"        <packing>"
"          <property name='left_attach'>0</property>"
"          <property name='top_attach'>0</property>"
"        </packing>"
"      </child>"
"      <child>"
"        <object class='GtkScrolledWindow' id='scrolledwindow'>"
"          <property name='visible'>True</property>"
"          <property name='can_focus'>True</property>"
"          <property name='hexpand'>True</property>"
"          <property name='vexpand'>True</property>"
"          <property name='shadow_type'>in</property>"
"          <child>"
"            <object class='GtkViewport' id='viewport'>"
"              <property name='visible'>True</property>"
"              <property name='can_focus'>False</property>"
"              <child>"
"                <object class='GtkDrawingArea' id='canvas'>"
"                  <property name='width_request'>600</property>"
"                  <property name='height_request'>600</property>"
"                  <property name='visible'>True</property>"
"                  <property name='app_paintable'>True</property>"
"                  <property name='can_focus'>False</property>"
"                </object>"
"              </child>"
"            </object>"
"          </child>"
"        </object>"
"        <packing>"
"          <property name='left_attach'>0</property>"
"          <property name='top_attach'>1</property>"
"        </packing>"
"      </child>"
"    </object>"
"  </child>"
"</object>"
"</interface>",
      -1);
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
