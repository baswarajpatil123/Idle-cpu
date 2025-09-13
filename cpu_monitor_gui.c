#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <sys/wait.h>
#include <mpg123.h>
#include <ao/ao.h>

// A structure to hold the different CPU time values read from /proc/stat
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CpuTimes;

// Global variables for monitoring state
static gboolean monitoring = FALSE;
static pthread_t monitor_thread;
static GtkWidget *status_label;
static GtkWidget *cpu_usage_label;
static GtkWidget *start_button;
static GtkWidget *stop_button;
static guint cpu_update_timer_id = 0;

// Function to read the aggregate CPU times from /proc/stat
void get_cpu_times(CpuTimes *times) {
    FILE *file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Error opening /proc/stat");
        return;
    }

    char line[256];
    // The first line "cpu ..." contains the aggregate values for all cores
    if (fgets(line, sizeof(line), file) != NULL) {
        // Parse the values from the line
        sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &times->user, &times->nice, &times->system, &times->idle,
               &times->iowait, &times->irq, &times->softirq, &times->steal);
    }
    fclose(file);
}

// Function to get current CPU usage percentage
double get_current_cpu_usage() {
    static CpuTimes prev_times = {0};
    static gboolean first_call = TRUE;
    
    CpuTimes curr_times;
    get_cpu_times(&curr_times);
    
    if (first_call) {
        prev_times = curr_times;
        first_call = FALSE;
        return 0.0;
    }
    
    // Calculate the total time spent in different states since the last measurement
    unsigned long long prev_idle_total = prev_times.idle + prev_times.iowait;
    unsigned long long prev_active_total = prev_times.user + prev_times.nice + prev_times.system + prev_times.irq + prev_times.softirq + prev_times.steal;
    unsigned long long prev_total = prev_idle_total + prev_active_total;
    
    unsigned long long curr_idle_total = curr_times.idle + curr_times.iowait;
    unsigned long long curr_active_total = curr_times.user + curr_times.nice + curr_times.system + curr_times.irq + curr_times.softirq + curr_times.steal;
    unsigned long long curr_total = curr_idle_total + curr_active_total;
    
    // Calculate the difference (delta) in total and idle times
    unsigned long long total_delta = curr_total - prev_total;
    unsigned long long idle_delta = curr_idle_total - prev_idle_total;
    
    // Calculate the CPU utilization percentage
    double cpu_utilization = 0.0;
    if (total_delta > 0) {
        cpu_utilization = (double)(total_delta - idle_delta) / total_delta * 100.0;
    }
    
    // Save the current times as the previous times for the next call
    prev_times = curr_times;
    
    return cpu_utilization;
}


// Function to play MP3 sound alert
void play_sound_alert() {
    mpg123_handle *mh;
    unsigned char *buffer;
    size_t buffer_size;
    size_t done;
    int err;
    int channels, encoding;
    long rate;
    ao_device *dev;
    ao_sample_format format;
    
    const char *mp3_file = "drums_of_liberation.mp3";
    
    // Initialize mpg123 library
    mpg123_init();
    mh = mpg123_new(NULL, &err);
    if (mh == NULL) {
        fprintf(stderr, "Unable to create mpg123 handle: %s\n", mpg123_plain_strerror(err));
        return;
    }
    
    // Open the MP3 file
    if (mpg123_open(mh, mp3_file) != MPG123_OK) {
        fprintf(stderr, "Error opening file: %s\n", mp3_file);
        mpg123_delete(mh);
        mpg123_exit();
        return;
    }
    
    // Get audio format information
    if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK) {
        fprintf(stderr, "Error getting audio format\n");
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        return;
    }
    
    // Set the output format
    format.bits = mpg123_encsize(encoding) * 8;
    format.rate = rate;
    format.channels = channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    
    // Initialize libao
    ao_initialize();
    dev = ao_open_live(ao_default_driver_id(), &format, NULL);
    if (dev == NULL) {
        fprintf(stderr, "Error opening audio device\n");
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
        ao_shutdown();
        return;
    }
    
    // Allocate buffer
    buffer_size = mpg123_outblock(mh);
    buffer = (unsigned char *)malloc(buffer_size * sizeof(unsigned char));
    
    // Decode and play the MP3 file
    while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
        ao_play(dev, (char *)buffer, done);
    }
    
    // Clean up
    free(buffer);
    ao_close(dev);
    ao_shutdown();
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
}

// Wrapper function for updating status label from idle callback
static gboolean update_status_callback(gpointer data) {
    gchar *text = (gchar*)data;
    gtk_label_set_text(GTK_LABEL(status_label), text);
    g_free(text);
    return FALSE; // Don't repeat
}

// Wrapper function for updating CPU usage label from idle callback
static gboolean update_cpu_usage_callback(gpointer data) {
    gchar *text = (gchar*)data;
    gtk_label_set_text(GTK_LABEL(cpu_usage_label), text);
    g_free(text);
    return FALSE; // Don't repeat
}

// Function to update status label (thread-safe)
void update_status_label(const char *text) {
    gchar *text_copy = g_strdup(text);
    g_idle_add(update_status_callback, text_copy);
}

// Function to update CPU usage label (thread-safe)
void update_cpu_usage_label() {
    double cpu_usage = get_current_cpu_usage();
    char usage_text[64];
    snprintf(usage_text, sizeof(usage_text), "Current CPU Usage: %.1f%%", cpu_usage);
    
    gchar *text_copy = g_strdup(usage_text);
    g_idle_add(update_cpu_usage_callback, text_copy);
}

// CPU monitoring thread function
void* cpu_monitor_thread(void *arg) {
    (void)arg; // Suppress unused parameter warning
    CpuTimes prev_times, curr_times;
    double cpu_readings[30]; // Store 30 seconds of readings
    int reading_index = 0;
    int readings_count = 0;
    int consecutive_low_count = 0; // Count consecutive seconds below 30%
    
    // Get the initial CPU times to establish a baseline
    get_cpu_times(&prev_times);
    
    update_status_label("Monitoring CPU usage...");
    
    while (monitoring) {
        // Wait for one second before the next measurement
        sleep(1);
        
        if (!monitoring) break;
        
        // Get the new CPU times
        get_cpu_times(&curr_times);
        
        // Calculate the total time spent in different states since the last measurement
        unsigned long long prev_idle_total = prev_times.idle + prev_times.iowait;
        unsigned long long prev_active_total = prev_times.user + prev_times.nice + prev_times.system + prev_times.irq + prev_times.softirq + prev_times.steal;
        unsigned long long prev_total = prev_idle_total + prev_active_total;
        
        unsigned long long curr_idle_total = curr_times.idle + curr_times.iowait;
        unsigned long long curr_active_total = curr_times.user + curr_times.nice + curr_times.system + curr_times.irq + curr_times.softirq + curr_times.steal;
        unsigned long long curr_total = curr_idle_total + curr_active_total;
        
        // Calculate the difference (delta) in total and idle times
        unsigned long long total_delta = curr_total - prev_total;
        unsigned long long idle_delta = curr_idle_total - prev_idle_total;
        
        // Calculate the CPU utilization percentage
        double cpu_utilization = 0.0;
        if (total_delta > 0) {
            cpu_utilization = (double)(total_delta - idle_delta) / total_delta * 100.0;
        }
        
        // Store the reading
        cpu_readings[reading_index] = cpu_utilization;
        reading_index = (reading_index + 1) % 30;
        if (readings_count < 30) {
            readings_count++;
        }
        
        // Calculate average if we have enough readings
        if (readings_count >= 30) {
            double sum = 0.0;
            for (int i = 0; i < 30; i++) {
                sum += cpu_readings[i];
            }
            double average_cpu = sum / 30.0;
            
            // Update status with current and average readings
            char status_text[256];
            snprintf(status_text, sizeof(status_text), 
                    "Current: %.1f%% | 30s Avg: %.1f%% | Low Count: %d/30", 
                    cpu_utilization, average_cpu, consecutive_low_count);
            update_status_label(status_text);
            
            // Check if current CPU usage is above 30% - reset timer
            if (cpu_utilization > 30.0) {
                consecutive_low_count = 0; // Reset the counter
            } else {
                consecutive_low_count++;
                // Play sound if we've had 30 consecutive seconds below 30%
                if (consecutive_low_count >= 30) {
                    play_sound_alert();
                    consecutive_low_count = 0; // Reset after alert
                }
            }
        } else {
            // Show progress while collecting readings
            char status_text[256];
            snprintf(status_text, sizeof(status_text), 
                    "Current: %.1f%% | Collecting data... (%d/30)", 
                    cpu_utilization, readings_count);
            update_status_label(status_text);
        }
        
        // Save the current times as the previous times for the next loop iteration
        prev_times = curr_times;
    }
    
    update_status_label("Monitoring stopped");
    return NULL;
}

// Timer callback for continuous CPU usage updates
static gboolean cpu_update_timer(gpointer data) {
    (void)data; // Suppress unused parameter warning
    update_cpu_usage_label();
    return TRUE; // Continue the timer
}

// Callback function for the Start button
static void on_start_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (!monitoring) {
        monitoring = TRUE;
        gtk_widget_set_sensitive(start_button, FALSE);
        gtk_widget_set_sensitive(stop_button, TRUE);
        
        // Start continuous CPU usage updates (every 1 second)
        cpu_update_timer_id = g_timeout_add_seconds(1, cpu_update_timer, NULL);
        
        // Start the monitoring thread
        if (pthread_create(&monitor_thread, NULL, cpu_monitor_thread, NULL) != 0) {
            g_print("Error creating monitoring thread\n");
            monitoring = FALSE;
            gtk_widget_set_sensitive(start_button, TRUE);
            gtk_widget_set_sensitive(stop_button, FALSE);
            g_source_remove(cpu_update_timer_id);
            cpu_update_timer_id = 0;
        }
    }
}

// Callback function for the Stop button
static void on_stop_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (monitoring) {
        monitoring = FALSE;
        gtk_widget_set_sensitive(start_button, TRUE);
        gtk_widget_set_sensitive(stop_button, FALSE);
        
        // Stop the CPU update timer
        if (cpu_update_timer_id != 0) {
            g_source_remove(cpu_update_timer_id);
            cpu_update_timer_id = 0;
        }
        
        // Wait for the monitoring thread to finish
        pthread_join(monitor_thread, NULL);
    }
}

// Callback function for window close
static void on_window_destroy(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (monitoring) {
        monitoring = FALSE;
        pthread_join(monitor_thread, NULL);
    }
    
    // Stop the CPU update timer
    if (cpu_update_timer_id != 0) {
        g_source_remove(cpu_update_timer_id);
        cpu_update_timer_id = 0;
    }
    
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *title_label;
    
    gtk_init(&argc, &argv);
    
    // Create the main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CPU Monitor - Idle Alert");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 200);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);
    
    // Connect the destroy signal
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Create a grid layout
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 15);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
    gtk_container_add(GTK_CONTAINER(window), grid);
    
    // Create title label
    title_label = gtk_label_new("CPU Usage Monitor");
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size='large' weight='bold'>CPU Usage Monitor</span>");
    gtk_grid_attach(GTK_GRID(grid), title_label, 0, 0, 2, 1);
    
    // Create CPU usage label
    cpu_usage_label = gtk_label_new("Current CPU Usage: 0.0%");
    gtk_label_set_markup(GTK_LABEL(cpu_usage_label), "<span size='medium' weight='bold'>Current CPU Usage: 0.0%</span>");
    gtk_grid_attach(GTK_GRID(grid), cpu_usage_label, 0, 1, 2, 1);
    
    // Create status label
    status_label = gtk_label_new("Ready to monitor");
    gtk_label_set_markup(GTK_LABEL(status_label), "<span size='medium'>Ready to monitor</span>");
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 2, 2, 1);
    
    // Create Start button
    start_button = gtk_button_new_with_label("Start Monitoring");
    gtk_widget_set_size_request(start_button, 150, 50);
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), start_button, 0, 3, 1, 1);
    
    // Create Stop button
    stop_button = gtk_button_new_with_label("Stop Monitoring");
    gtk_widget_set_size_request(stop_button, 150, 50);
    gtk_widget_set_sensitive(stop_button, FALSE); // Initially disabled
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid), stop_button, 1, 3, 1, 1);
    
    // Create info label
    GtkWidget *info_label = gtk_label_new("Alert: Sound plays when CPU usage < 30% for 30 consecutive seconds");
    gtk_label_set_markup(GTK_LABEL(info_label), "<span size='small' style='italic'>Alert: Sound plays when CPU usage &lt; 30% for 30 consecutive seconds</span>");
    gtk_grid_attach(GTK_GRID(grid), info_label, 0, 4, 2, 1);
    
    // Show all widgets
    gtk_widget_show_all(window);
    
    // Start the GTK main loop
    gtk_main();
    
    return 0;
}
