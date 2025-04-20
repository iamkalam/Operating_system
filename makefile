# Makefile for CPU Hotplug Governor

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lm -pthread

# Target executable name
TARGET = cpu-hotplug-governor

# Source files
SRCS = cpu-hotplug-governor.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(TARGET) $(OBJS)

# Install the program
install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin/
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/

# Uninstall the program
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

# Phony targets
.PHONY: all clean install uninstall
