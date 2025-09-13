#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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

// Function to read the aggregate CPU times from /proc/stat
void get_cpu_times(CpuTimes *times) {
    FILE *file = fopen("/proc/stat", "r");
    if (file == NULL) {
        perror("Error opening /proc/stat");
        exit(1);
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

int main() {
    CpuTimes prev_times, curr_times;

    // Get the initial CPU times to establish a baseline
    get_cpu_times(&prev_times);

    // Main loop to continuously monitor CPU
    while (1) {
        // Wait for one second before the next measurement
        sleep(1);

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

        // Display the current CPU utilization. The '\r' moves the cursor to the
        // beginning of the line, creating a real-time update effect.
        printf("\rCPU Utilization: %.2f%% ", cpu_utilization);
        fflush(stdout); // Force the output to be written to the console

        // Check if the utilization is below the 30% threshold
        if (cpu_utilization < 30.0) {
            printf("\a"); // Make a beep sound using the ASCII bell character
        }

        // Save the current times as the previous times for the next loop iteration
        prev_times = curr_times;
    }

    return 0;
}