#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_CORES 128
#define MAX_PATH 256
#define REFRESH_INTERVAL 1000 // Refresh UI every 1 second

typedef struct
{
    int id;
    bool online;
    GtkWidget *toggle;
    GtkWidget *status_label;
    GtkWidget *load_label;
    GtkWidget *freq_label;
    GtkWidget *temp_label;
    GtkWidget *recommendation_icon;
} CoreInfo;

typedef struct
{
    CoreInfo cores[MAX_CORES];
    int num_cores;
    GtkWidget *window;
    GtkWidget *status_bar;
    GtkWidget *power_profile_combo;
    GtkWidget *save_profile_button;
    GtkWidget *apply_recommendations_button;
    GtkWidget *auto_toggle;
    GtkWidget *cpu_usage_chart;
    GtkWidget *power_usage_chart;
    GtkWidget *temperature_chart;
    gboolean auto_mode;
    int refresh_timeout_id;
    pthread_mutex_t mutex;
} AppState;

AppState app;

// Function prototypes
static void toggle_core(GtkWidget *widget, gpointer data);
static gboolean update_ui(gpointer data);
static void load_core_info();
static bool read_core_online_status(int core_id);
static bool set_core_online_status(int core_id, bool online);
static double get_core_load(int core_id);
static double get_core_frequency(int core_id);
static double get_core_temperature(int core_id);
static void apply_recommendations();
static void toggle_auto_mode(GtkWidget *widget, gpointer data);
static void save_profile(GtkWidget *widget, gpointer data);
static void load_profile(GtkWidget *widget, gpointer data);
static void change_power_profile(GtkWidget *widget, gpointer data);
static void draw_chart(GtkWidget *widget, cairo_t *cr, gpointer data);
static void activate_recommended_cores();

// Get the number of CPU cores on the system
static int get_num_cores()
{
    FILE *fp = popen("grep -c processor /proc/cpuinfo", "r");
    if (!fp)
        return 1;

    char buffer[32];
    fgets(buffer, sizeof(buffer), fp);
    pclose(fp);

    return atoi(buffer);
}

// Check if a file exists
static bool file_exists(const char *filename)
{
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// Read the online status of a CPU core
static bool read_core_online_status(int core_id)
{
    if (core_id == 0)
        return true; // Core 0 is always online

    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "/sys/devices/system/cpu/cpu%d/online", core_id);

    if (!file_exists(path))
        return false;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;

    char status;
    fscanf(fp, "%c", &status);
    fclose(fp);

    return status == '1';
}

// Set the online status of a CPU core
static bool set_core_online_status(int core_id, bool online)
{
    if (core_id == 0)
        return true; // Can't toggle core 0

    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "/sys/devices/system/cpu/cpu%d/online", core_id);

    if (!file_exists(path))
        return false;

    FILE *fp = fopen(path, "w");
    if (!fp)
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app.window),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "Failed to toggle CPU%d. Do you have root privileges?", core_id);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return false;
    }

    fprintf(fp, "%d", online ? 1 : 0);
    fclose(fp);

    return true;
}

// Get CPU load for a specific core
static double get_core_load(int core_id)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "grep 'cpu%d ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage}'", core_id);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return 0.0;

    char buffer[32];
    fgets(buffer, sizeof(buffer), fp);
    pclose(fp);

    return atof(buffer);
}

// Get CPU frequency for a specific core
static double get_core_frequency(int core_id)
{
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core_id);

    if (!file_exists(path))
        return 0.0;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0.0;

    long freq;
    fscanf(fp, "%ld", &freq);
    fclose(fp);

    return freq / 1000.0; // Convert to MHz
}

// Get CPU temperature for a specific core (if available)
static double get_core_temperature(int core_id)
{
    // This is a simplified implementation; real-world would need to check various temperature sources
    char path[MAX_PATH];
    snprintf(path, MAX_PATH, "/sys/class/thermal/thermal_zone0/temp");

    if (!file_exists(path))
        return 0.0;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0.0;

    long temp;
    fscanf(fp, "%ld", &temp);
    fclose(fp);

    return temp / 1000.0; // Convert to °C
}

// Toggle a CPU core on/off
static void toggle_core(GtkWidget *widget, gpointer data)
{
    int core_id = GPOINTER_TO_INT(data);
    bool new_status = gtk_switch_get_active(GTK_SWITCH(widget));

    if (set_core_online_status(core_id, new_status))
    {
        app.cores[core_id].online = new_status;
        gtk_label_set_text(GTK_LABEL(app.cores[core_id].status_label),
                           new_status ? "Online" : "Offline");

        if (new_status)
        {
            gtk_widget_set_sensitive(app.cores[core_id].load_label, TRUE);
            gtk_widget_set_sensitive(app.cores[core_id].freq_label, TRUE);
            gtk_widget_set_sensitive(app.cores[core_id].temp_label, TRUE);
        }
        else
        {
            gtk_label_set_text(GTK_LABEL(app.cores[core_id].load_label), "N/A");
            gtk_label_set_text(GTK_LABEL(app.cores[core_id].freq_label), "N/A");
            gtk_label_set_text(GTK_LABEL(app.cores[core_id].temp_label), "N/A");
            gtk_widget_set_sensitive(app.cores[core_id].load_label, FALSE);
            gtk_widget_set_sensitive(app.cores[core_id].freq_label, FALSE);
            gtk_widget_set_sensitive(app.cores[core_id].temp_label, FALSE);
        }

        // Update statusbar
        char message[64];
        snprintf(message, sizeof(message), "CPU%d turned %s", core_id, new_status ? "online" : "offline");
        gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, message);
    }
    else
    {
        // Revert toggle switch if operation failed
        gtk_switch_set_active(GTK_SWITCH(widget), app.cores[core_id].online);
    }
}

// Load CPU core information
static void load_core_info()
{
    app.num_cores = get_num_cores();
    if (app.num_cores > MAX_CORES)
        app.num_cores = MAX_CORES;

    for (int i = 0; i < app.num_cores; i++)
    {
        app.cores[i].id = i;
        app.cores[i].online = read_core_online_status(i);
    }
}

// Determine if a core should be active based on system load
static bool should_be_active(int core_id)
{
    static double total_load = 0.0;
    static int active_cores = 0;

    // Simple heuristic: If overall load > 70% of active cores, suggest activating more
    // If load < 30% of active cores, suggest deactivating some

    // Count active cores and get total load
    total_load = 0.0;
    active_cores = 0;

    for (int i = 0; i < app.num_cores; i++)
    {
        if (app.cores[i].online)
        {
            active_cores++;
            total_load += get_core_load(i);
        }
    }

    // Always keep core 0 online
    if (core_id == 0)
        return true;

    // If this core is already online
    if (app.cores[core_id].online)
    {
        // Keep it online if load is high
        return (total_load / active_cores) > 30.0;
    }
    else
    {
        // Turn it on if load is high
        return active_cores > 0 && (total_load / active_cores) > 70.0;
    }
}

// Apply automatic core recommendations
static void apply_recommendations()
{
    for (int i = 1; i < app.num_cores; i++)
    { // Skip core 0
        bool should_be_on = should_be_active(i);

        if (app.cores[i].online != should_be_on)
        {
            if (set_core_online_status(i, should_be_on))
            {
                app.cores[i].online = should_be_on;
                gtk_switch_set_active(GTK_SWITCH(app.cores[i].toggle), should_be_on);
            }
        }
    }

    // Update status bar
    gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, "Applied automatic core recommendations");
}

// Handle auto mode toggling
static void toggle_auto_mode(GtkWidget *widget, gpointer data)
{
    app.auto_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (app.auto_mode)
    {
        gtk_widget_set_sensitive(app.apply_recommendations_button, FALSE);
        apply_recommendations();
        gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, "Auto Mode: ON - Cores will be managed automatically");
    }
    else
    {
        gtk_widget_set_sensitive(app.apply_recommendations_button, TRUE);
        gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, "Auto Mode: OFF - Manual control enabled");
    }
}

// Save current core configuration to a profile
static void save_profile(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Save Profile",
                                         GTK_WINDOW(app.window),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Save", GTK_RESPONSE_ACCEPT,
                                         NULL);
    chooser = GTK_FILE_CHOOSER(dialog);

    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    gtk_file_chooser_set_current_name(chooser, "cpu_profile.conf");

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *filename;
        filename = gtk_file_chooser_get_filename(chooser);

        FILE *fp = fopen(filename, "w");
        if (fp)
        {
            for (int i = 0; i < app.num_cores; i++)
            {
                fprintf(fp, "%d:%d\n", i, app.cores[i].online ? 1 : 0);
            }
            fclose(fp);

            char message[256];
            snprintf(message, sizeof(message), "Profile saved to %s", filename);
            gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, message);
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Load a saved profile
static void load_profile(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Load Profile",
                                         GTK_WINDOW(app.window),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL);
    chooser = GTK_FILE_CHOOSER(dialog);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        char *filename;
        filename = gtk_file_chooser_get_filename(chooser);

        FILE *fp = fopen(filename, "r");
        if (fp)
        {
            int core_id, status;
            char line[64];

            while (fgets(line, sizeof(line), fp))
            {
                if (sscanf(line, "%d:%d", &core_id, &status) == 2)
                {
                    if (core_id >= 0 && core_id < app.num_cores && core_id > 0)
                    { // Don't touch core 0
                        bool online = status != 0;

                        if (set_core_online_status(core_id, online))
                        {
                            app.cores[core_id].online = online;
                            gtk_switch_set_active(GTK_SWITCH(app.cores[core_id].toggle), online);
                        }
                    }
                }
            }

            fclose(fp);

            char message[256];
            snprintf(message, sizeof(message), "Profile loaded from %s", filename);
            gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, message);
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Change the power profile
static void change_power_profile(GtkWidget *widget, gpointer data)
{
    const char *profile = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));

    if (strcmp(profile, "Power Saver") == 0)
    {
        // Turn off all cores except 0 and 1
        for (int i = 2; i < app.num_cores; i++)
        {
            if (app.cores[i].online)
            {
                set_core_online_status(i, false);
                app.cores[i].online = false;
                gtk_switch_set_active(GTK_SWITCH(app.cores[i].toggle), false);
            }
        }
    }
    else if (strcmp(profile, "Balanced") == 0)
    {
        // Turn on about half the cores
        for (int i = 0; i < app.num_cores; i++)
        {
            bool should_be_on = i < (app.num_cores + 1) / 2;

            if (app.cores[i].online != should_be_on)
            {
                set_core_online_status(i, should_be_on);
                app.cores[i].online = should_be_on;
                gtk_switch_set_active(GTK_SWITCH(app.cores[i].toggle), should_be_on);
            }
        }
    }
    else if (strcmp(profile, "Performance") == 0)
    {
        // Turn on all cores
        for (int i = 0; i < app.num_cores; i++)
        {
            if (!app.cores[i].online)
            {
                set_core_online_status(i, true);
                app.cores[i].online = true;
                gtk_switch_set_active(GTK_SWITCH(app.cores[i].toggle), true);
            }
        }
    }

    char message[64];
    snprintf(message, sizeof(message), "Applied %s profile", profile);
    gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, message);
}

// Draw a chart for CPU stats
static void draw_chart(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    const int chart_type = GPOINTER_TO_INT(data); // 0 = usage, 1 = power, 2 = temp

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    const int width = allocation.width;
    const int height = allocation.height;

    // Draw background
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    // Draw border
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_stroke(cr);

    // Draw chart title
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);

    const char *title;
    switch (chart_type)
    {
    case 0:
        title = "CPU Usage (%)";
        break;
    case 1:
        title = "Power Usage (Relative)";
        break;
    case 2:
        title = "Temperature (°C)";
        break;
    default:
        title = "Unknown Chart";
        break;
    }

    cairo_text_extents_t extents;
    cairo_text_extents(cr, title, &extents);
    cairo_move_to(cr, (width - extents.width) / 2, 20);
    cairo_show_text(cr, title);

    // Draw bars for each core
    const int bar_padding = 5;
    const int bar_width = (width - (app.num_cores + 1) * bar_padding) / app.num_cores;
    const int chart_top = 30;
    const int chart_bottom = height - 30;
    const int chart_height = chart_bottom - chart_top;

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);

    for (int i = 0; i < app.num_cores; i++)
    {
        if (!app.cores[i].online)
        {
            // Draw inactive bar
            cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
            cairo_rectangle(cr,
                            bar_padding + i * (bar_width + bar_padding),
                            chart_bottom - 2,
                            bar_width,
                            2);
            cairo_fill(cr);

            // Draw "Offline" text
            cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
            cairo_move_to(cr,
                          bar_padding + i * (bar_width + bar_padding) + bar_width / 2 - 15,
                          chart_bottom - 5);
            cairo_show_text(cr, "Offline");
            continue;
        }

        double value = 0.0;
        switch (chart_type)
        {
        case 0:
            value = get_core_load(i);
            cairo_set_source_rgb(cr, 0.2, 0.6, 0.9);
            break;
        case 1:
            value = get_core_frequency(i) / 1000.0 * 100.0; // Normalized to percentage
            if (value > 100.0)
                value = 100.0;
            cairo_set_source_rgb(cr, 0.9, 0.6, 0.2);
            break;
        case 2:
            value = get_core_temperature(i);
            value = (value / 80.0) * 100.0; // Normalized to percentage (80C = 100%)
            if (value > 100.0)
                value = 100.0;
            cairo_set_source_rgb(cr, 0.9, 0.2, 0.2);
            break;
        }

        const int bar_height = (int)((value / 100.0) * chart_height);

        // Draw the bar
        cairo_rectangle(cr,
                        bar_padding + i * (bar_width + bar_padding),
                        chart_bottom - bar_height,
                        bar_width,
                        bar_height);
        cairo_fill(cr);

        // Draw the value
        char value_text[16];
        switch (chart_type)
        {
        case 0:
            snprintf(value_text, sizeof(value_text), "%.1f%%", value);
            break;
        case 1:
            snprintf(value_text, sizeof(value_text), "%.2f GHz", get_core_frequency(i) / 1000.0);
            break;
        case 2:
            snprintf(value_text, sizeof(value_text), "%.1f°C", get_core_temperature(i));
            break;
        }

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_text_extents(cr, value_text, &extents);
        cairo_move_to(cr,
                      bar_padding + i * (bar_width + bar_padding) + (bar_width - extents.width) / 2,
                      chart_bottom - bar_height - 5);
        cairo_show_text(cr, value_text);

        // Draw the CPU number
        char cpu_text[8];
        snprintf(cpu_text, sizeof(cpu_text), "CPU%d", i);

        cairo_text_extents(cr, cpu_text, &extents);
        cairo_move_to(cr,
                      bar_padding + i * (bar_width + bar_padding) + (bar_width - extents.width) / 2,
                      chart_bottom + 15);
        cairo_show_text(cr, cpu_text);
    }
}

// Update the UI with current core information
static gboolean update_ui(gpointer data)
{
    pthread_mutex_lock(&app.mutex);

    for (int i = 0; i < app.num_cores; i++)
    {
        app.cores[i].online = read_core_online_status(i);

        // Update the toggle switch state (without triggering the callback)
        g_signal_handlers_block_by_func(app.cores[i].toggle, G_CALLBACK(toggle_core), GINT_TO_POINTER(i));
        gtk_switch_set_active(GTK_SWITCH(app.cores[i].toggle), app.cores[i].online);
        g_signal_handlers_unblock_by_func(app.cores[i].toggle, G_CALLBACK(toggle_core), GINT_TO_POINTER(i));

        // Update status label
        gtk_label_set_text(GTK_LABEL(app.cores[i].status_label),
                           app.cores[i].online ? "Online" : "Offline");

        // Update stats for online cores
        if (app.cores[i].online)
        {
            double load = get_core_load(i);
            double freq = get_core_frequency(i);
            double temp = get_core_temperature(i);

            char load_text[32], freq_text[32], temp_text[32];
            snprintf(load_text, sizeof(load_text), "%.1f%%", load);
            snprintf(freq_text, sizeof(freq_text), "%.2f GHz", freq / 1000.0);
            snprintf(temp_text, sizeof(temp_text), "%.1f°C", temp);

            gtk_label_set_text(GTK_LABEL(app.cores[i].load_label), load_text);
            gtk_label_set_text(GTK_LABEL(app.cores[i].freq_label), freq_text);
            gtk_label_set_text(GTK_LABEL(app.cores[i].temp_label), temp_text);

            // Update recommendation icon
            if (should_be_active(i))
            {
                gtk_image_set_from_icon_name(GTK_IMAGE(app.cores[i].recommendation_icon),
                                             "emblem-ok", GTK_ICON_SIZE_SMALL_TOOLBAR);
            }
            else
            {
                gtk_image_set_from_icon_name(GTK_IMAGE(app.cores[i].recommendation_icon),
                                             "process-stop", GTK_ICON_SIZE_SMALL_TOOLBAR);
            }

            gtk_widget_set_sensitive(app.cores[i].load_label, TRUE);
            gtk_widget_set_sensitive(app.cores[i].freq_label, TRUE);
            gtk_widget_set_sensitive(app.cores[i].temp_label, TRUE);
        }
        else
        {
            gtk_label_set_text(GTK_LABEL(app.cores[i].load_label), "N/A");
            gtk_label_set_text(GTK_LABEL(app.cores[i].freq_label), "N/A");
            gtk_label_set_text(GTK_LABEL(app.cores[i].temp_label), "N/A");

            gtk_widget_set_sensitive(app.cores[i].load_label, FALSE);
            gtk_widget_set_sensitive(app.cores[i].freq_label, FALSE);
            gtk_widget_set_sensitive(app.cores[i].temp_label, FALSE);

            // Update recommendation icon for offline cores
            if (should_be_active(i))
            {
                gtk_image_set_from_icon_name(GTK_IMAGE(app.cores[i].recommendation_icon),
                                             "emblem-important", GTK_ICON_SIZE_SMALL_TOOLBAR);
            }
            else
            {
                gtk_image_set_from_icon_name(GTK_IMAGE(app.cores[i].recommendation_icon),
                                             "emblem-ok", GTK_ICON_SIZE_SMALL_TOOLBAR);
            }
        }
    }

    // Refresh charts
    gtk_widget_queue_draw(app.cpu_usage_chart);
    gtk_widget_queue_draw(app.power_usage_chart);
    gtk_widget_queue_draw(app.temperature_chart);

    // Apply recommendations if in auto mode
    if (app.auto_mode)
    {
        apply_recommendations();
    }

    pthread_mutex_unlock(&app.mutex);

    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *main_vbox, *header_box, *content_box, *chart_box;
    GtkWidget *title_label, *desc_label;
    GtkWidget *scrolled_window, *cores_grid;
    GtkWidget *control_panel, *profile_box;
    GtkWidget *load_profile_button;

    gtk_init(&argc, &argv);

    // Initialize mutex
    pthread_mutex_init(&app.mutex, NULL);

    // Load core information
    load_core_info();

    // Create the main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CPU Hotplug Governor");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    app.window = window;

    // Create main vertical box
    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 15);
    gtk_container_add(GTK_CONTAINER(window), main_vbox);

    // Header section
    header_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_vbox), header_box, FALSE, FALSE, 0);

    title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), "<span font_weight='bold' font_size='large'>CPU Hotplug Governor</span>");
    gtk_box_pack_start(GTK_BOX(header_box), title_label, FALSE, FALSE, 0);

    desc_label = gtk_label_new("Manage your CPU cores to optimize power consumption and performance");
    gtk_box_pack_start(GTK_BOX(header_box), desc_label, FALSE, FALSE, 0);

    // Control panel (auto toggle, profiles, etc.)
    control_panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), control_panel, FALSE, FALSE, 0);

    // Auto mode toggle
    app.auto_toggle = gtk_check_button_new_with_label("Auto Mode");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.auto_toggle), FALSE);
    g_signal_connect(app.auto_toggle, "toggled", G_CALLBACK(toggle_auto_mode), NULL);
    gtk_box_pack_start(GTK_BOX(control_panel), app.auto_toggle, FALSE, FALSE, 0);

    // Apply recommendations button
    app.apply_recommendations_button = gtk_button_new_with_label("Apply Recommendations");
    g_signal_connect(app.apply_recommendations_button, "clicked", G_CALLBACK(apply_recommendations), NULL);
    gtk_box_pack_start(GTK_BOX(control_panel), app.apply_recommendations_button, FALSE, FALSE, 0);

    // Profile management
    profile_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_end(GTK_BOX(control_panel), profile_box, FALSE, FALSE, 0);

    // Power profile selector
    GtkWidget *profile_label = gtk_label_new("Power Profile:");
    gtk_box_pack_start(GTK_BOX(profile_box), profile_label, FALSE, FALSE, 0);

    app.power_profile_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.power_profile_combo), "Power Saver");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.power_profile_combo), "Balanced");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.power_profile_combo), "Performance");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app.power_profile_combo), 1); // Set "Balanced" as default
    g_signal_connect(app.power_profile_combo, "changed", G_CALLBACK(change_power_profile), NULL);
    gtk_box_pack_start(GTK_BOX(profile_box), app.power_profile_combo, FALSE, FALSE, 0);

    // Save/Load profile buttons
    app.save_profile_button = gtk_button_new_with_label("Save Profile");
    g_signal_connect(app.save_profile_button, "clicked", G_CALLBACK(save_profile), NULL);
    gtk_box_pack_start(GTK_BOX(profile_box), app.save_profile_button, FALSE, FALSE, 0);

    load_profile_button = gtk_button_new_with_label("Load Profile");
    g_signal_connect(load_profile_button, "clicked", G_CALLBACK(load_profile), NULL);
    gtk_box_pack_start(GTK_BOX(profile_box), load_profile_button, FALSE, FALSE, 0);

    // Create content box for charts and core controls
    content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), content_box, TRUE, TRUE, 0);

    // Charts box
    chart_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(content_box), chart_box, TRUE, TRUE, 0);

    // CPU Usage Chart
    app.cpu_usage_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.cpu_usage_chart, 250, 200);
    g_signal_connect(app.cpu_usage_chart, "draw", G_CALLBACK(draw_chart), GINT_TO_POINTER(0));
    gtk_box_pack_start(GTK_BOX(chart_box), app.cpu_usage_chart, TRUE, TRUE, 0);

    // Power Usage Chart
    app.power_usage_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.power_usage_chart, 250, 200);
    g_signal_connect(app.power_usage_chart, "draw", G_CALLBACK(draw_chart), GINT_TO_POINTER(1));
    gtk_box_pack_start(GTK_BOX(chart_box), app.power_usage_chart, TRUE, TRUE, 0);

    // Temperature Chart
    app.temperature_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(app.temperature_chart, 250, 200);
    g_signal_connect(app.temperature_chart, "draw", G_CALLBACK(draw_chart), GINT_TO_POINTER(2));
    gtk_box_pack_start(GTK_BOX(chart_box), app.temperature_chart, TRUE, TRUE, 0);

    // Create scrolled window for cores grid
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(content_box), scrolled_window, TRUE, TRUE, 0);

    // Create grid for CPU cores
    cores_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(cores_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(cores_grid), 15);
    gtk_container_add(GTK_CONTAINER(scrolled_window), cores_grid);

    // Add headers
    GtkWidget *header_core = gtk_label_new("CPU Core");
    gtk_widget_set_halign(header_core, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(cores_grid), header_core, 0, 0, 1, 1);

    GtkWidget *header_toggle = gtk_label_new("Toggle");
    gtk_widget_set_halign(header_toggle, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cores_grid), header_toggle, 1, 0, 1, 1);

    GtkWidget *header_status = gtk_label_new("Status");
    gtk_widget_set_halign(header_status, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cores_grid), header_status, 2, 0, 1, 1);

    GtkWidget *header_load = gtk_label_new("Load");
    gtk_widget_set_halign(header_load, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cores_grid), header_load, 3, 0, 1, 1);

    GtkWidget *header_freq = gtk_label_new("Frequency");
    gtk_widget_set_halign(header_freq, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cores_grid), header_freq, 4, 0, 1, 1);

    GtkWidget *header_temp = gtk_label_new("Temperature");
    gtk_widget_set_halign(header_temp, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cores_grid), header_temp, 5, 0, 1, 1);

    GtkWidget *header_recommended = gtk_label_new("Recommended");
    gtk_widget_set_halign(header_recommended, GTK_ALIGN_CENTER);
    gtk_grid_attach(GTK_GRID(cores_grid), header_recommended, 6, 0, 1, 1);

    // Add controls for each CPU core
    for (int i = 0; i < app.num_cores; i++)
    {
        // Core label
        char core_label[16];
        snprintf(core_label, sizeof(core_label), "CPU %d", i);
        GtkWidget *label = gtk_label_new(core_label);
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(cores_grid), label, 0, i + 1, 1, 1);

        // Toggle switch
        app.cores[i].toggle = gtk_switch_new();
        gtk_switch_set_active(GTK_SWITCH(app.cores[i].toggle), app.cores[i].online);
        gtk_widget_set_halign(app.cores[i].toggle, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(cores_grid), app.cores[i].toggle, 1, i + 1, 1, 1);
        g_signal_connect(app.cores[i].toggle, "state-set", G_CALLBACK(toggle_core), GINT_TO_POINTER(i));

        // Core 0 cannot be disabled
        if (i == 0)
        {
            gtk_widget_set_sensitive(app.cores[i].toggle, FALSE);
        }

        // Status label
        app.cores[i].status_label = gtk_label_new(app.cores[i].online ? "Online" : "Offline");
        gtk_widget_set_halign(app.cores[i].status_label, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(cores_grid), app.cores[i].status_label, 2, i + 1, 1, 1);

        // Load label
        app.cores[i].load_label = gtk_label_new(app.cores[i].online ? "0%" : "N/A");
        gtk_widget_set_halign(app.cores[i].load_label, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(cores_grid), app.cores[i].load_label, 3, i + 1, 1, 1);

        // Frequency label
        app.cores[i].freq_label = gtk_label_new(app.cores[i].online ? "0 GHz" : "N/A");
        gtk_widget_set_halign(app.cores[i].freq_label, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(cores_grid), app.cores[i].freq_label, 4, i + 1, 1, 1);

        // Temperature label
        app.cores[i].temp_label = gtk_label_new(app.cores[i].online ? "0°C" : "N/A");
        gtk_widget_set_halign(app.cores[i].temp_label, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(cores_grid), app.cores[i].temp_label, 5, i + 1, 1, 1);

        // Recommendation icon
        app.cores[i].recommendation_icon = gtk_image_new_from_icon_name("emblem-ok", GTK_ICON_SIZE_SMALL_TOOLBAR);
        gtk_widget_set_halign(app.cores[i].recommendation_icon, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(cores_grid), app.cores[i].recommendation_icon, 6, i + 1, 1, 1);

        if (!app.cores[i].online)
        {
            gtk_widget_set_sensitive(app.cores[i].load_label, FALSE);
            gtk_widget_set_sensitive(app.cores[i].freq_label, FALSE);
            gtk_widget_set_sensitive(app.cores[i].temp_label, FALSE);
        }
    }

    // Status bar
    app.status_bar = gtk_statusbar_new();
    gtk_box_pack_end(GTK_BOX(main_vbox), app.status_bar, FALSE, FALSE, 0);
    gtk_statusbar_push(GTK_STATUSBAR(app.status_bar), 0, "Ready. Found CPU cores: 0 to N");

    // Set up a timer to update the UI periodically
    app.refresh_timeout_id = g_timeout_add(REFRESH_INTERVAL, update_ui, NULL);

    // Show all widgets
    gtk_widget_show_all(window);

    // Start the GTK main loop
    gtk_main();

    // Clean up
    g_source_remove(app.refresh_timeout_id);
    pthread_mutex_destroy(&app.mutex);

    return 0;
}
