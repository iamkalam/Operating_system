CC = gcc
CFLAGS = -Wall -g

all: resource_monitor migration_manager process_migrator

resource_monitor: resource_monitor.c
    $(CC) $(CFLAGS) -o resource_monitor resource_monitor.c

migration_manager: migration_manager.c
    $(CC) $(CFLAGS) -o migration_manager migration_manager.c

process_migrator: process_migrator.c
    $(CC) $(CFLAGS) -o process_migrator process_migrator.c

clean:
    rm -f resource_monitor migration_manager process_migrator