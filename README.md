# CPU Monitor GUI - Idle Alert

A GTK-based GUI application that monitors CPU usage across all cores and plays a sound alert when the average CPU usage falls below 30% for 30 seconds.

## Features

- **GUI Interface**: Clean interface with Start/Stop buttons
- **Real-time Monitoring**: Shows current CPU usage and 30-second average
- **Multi-core Support**: Monitors all CPU cores collectively
- **Sound Alert**: Plays sound when average CPU usage < 30% for 30 seconds
- **Thread-safe**: Uses pthread for background monitoring

## Requirements

- Linux system
- GTK+ 3.0 development libraries
- GCC compiler
- pthread library

## Installation

### Install Dependencies (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y libgtk-3-dev build-essential
```

### Compile the Application
```bash
make
```

Or manually:
```bash
gcc -Wall -Wextra -std=c99 cpu_monitor_gui.c -o cpu_monitor_gui `pkg-config --cflags --libs gtk+-3.0` -lpthread
```

## Usage

1. **Run the application**:
   ```bash
   ./cpu_monitor_gui
   ```

2. **Start Monitoring**: Click the "Start Monitoring" button to begin CPU monitoring

3. **Stop Monitoring**: Click the "Stop Monitoring" button to stop monitoring

4. **Sound Alert**: The application will play a sound when the 30-second average CPU usage falls below 30%

## How It Works

1. **CPU Monitoring**: Reads `/proc/stat` to get CPU usage statistics
2. **30-Second Averaging**: Collects 30 readings (one per second) and calculates the average
3. **Alert System**: Plays sound alert when average CPU usage < 30%
4. **Real-time Display**: Shows both current CPU usage and 30-second average

## Makefile Targets

- `make` or `make all` - Build the application
- `make clean` - Remove build artifacts
- `make install-deps` - Install required dependencies
- `make run` - Build and run the application
- `make help` - Show help message

## Customization

You can modify the sound alert by changing the `play_sound_alert()` function in the source code:

```c
void play_sound_alert() {
    // Play a specific sound file
    system("aplay /path/to/your/sound.wav 2>/dev/null");
    
    // Or use a different alert method
    system("notify-send 'CPU Alert' 'CPU usage is below 30%'");
}
```

## Troubleshooting

- **Compilation errors**: Make sure GTK+ 3.0 development libraries are installed
- **No sound**: Check if your system has audio capabilities and ALSA is working
- **Permission issues**: The application needs read access to `/proc/stat`

## License

This project is open source and available under the MIT License.

