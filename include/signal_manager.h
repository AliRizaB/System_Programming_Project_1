// Work for Student 1
#ifndef SIGNAL_MANAGER_H
#define SIGNAL_MANAGER_H

#include <signal.h>

// Signal flags
extern volatile sig_atomic_t g_shutdown_requested; // SIGINT / SIGTERM , Ctrl+C / , kill -2 <PID> / kill -15 <PID>
extern volatile sig_atomic_t g_status_report_requested; // SIGUSR1, kill -10 <PID>
extern volatile sig_atomic_t g_shutdown_signal; // stores exact number of SIGINT / SIGTERM

int init_signal_handlers(void)

#endif