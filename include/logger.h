// Work for Student 3

#ifndef LOGGER_H
#define LOGGER_H

#include "process_manager.h"

// opens the log file (creates it if it does not exist)
int init_logger(const char *log_file_path);

// writes one line with timestamp: [YYYY-MM-DD HH:MM:SS] [EVENT_TYPE] message
void log_event(const char *event_type, const char *message);

// SIGUSR1 report, writes pid and state of every child
void log_status_report(const ChildProcess *children, int count);

void close_logger(void);

#endif
