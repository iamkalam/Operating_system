CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = pmms
SRC = pmms.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./pmms-monitor.sh

clean:
	rm -f $(TARGET)