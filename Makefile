CC = gcc
CFLAGS = -Wall -Wextra -std=c99
GTK_FLAGS = `pkg-config --cflags --libs gtk+-3.0`
PTHREAD_FLAGS = -lpthread
MP3_FLAGS = -lmpg123 -lao

# Target executable
TARGET = cpu_monitor_gui

# Source files
SOURCES = cpu_monitor_gui.c

# Default target
all: $(TARGET)

# Build the GUI application
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(GTK_FLAGS) $(PTHREAD_FLAGS) $(MP3_FLAGS)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libgtk-3-dev build-essential libmpg123-dev libao-dev

# Run the application
run: $(TARGET)
	./$(TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build the CPU monitor GUI application"
	@echo "  clean        - Remove build artifacts"
	@echo "  install-deps - Install required dependencies (Ubuntu/Debian)"
	@echo "  run          - Build and run the application"
	@echo "  help         - Show this help message"

.PHONY: all clean install-deps run help
