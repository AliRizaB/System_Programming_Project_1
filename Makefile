CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude

SRCS = src/main.c src/signal_manager.c src/process_manager.c src/logger.c
HEADERS = include/signal_manager.h include/process_manager.h include/logger.h

all: watchdog worker

# main program
watchdog: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -o watchdog $(SRCS)

# worker is started with execl("./worker", ...) so it has to be in the same folder
worker: workers/worker.c
	$(CC) $(CFLAGS) -o worker workers/worker.c

test: all
	bash tests/test_scenarios.sh

clean:
	rm -f watchdog worker watchdog.log

.PHONY: all test clean
