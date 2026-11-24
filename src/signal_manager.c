#define _POSIX_C_SOURCE 200809L // Enable POSIX.1-2008 APIs (sigaction, sigemptyset, sigaddset)

#include "signal_manager.h"
#include <string.h>
#include <stdio.h>

// Global Flags
volatile sig_atomic_t g_shutdown_requested = 0;
volatile sig_atomic_t g_status_report_requested = 0;
volatile sig_atomic_t g_shutdown_signal = 0;

// SIGINT and SIGTERM handler
static void handle_termination_signal(int signum) {
    g_shutdown_signal = signum; // SIGINT or SIGTERM
    g_shutdown_requested = 1;
}

// SIGUSER1 Status report
static void handle_usr1_signal(int signum) {
    (void)signum; // parameter is supressed
    g_status_report_requested = 1;
}

int init_signal_handlers(void) {
    struct sigaction sa_term;
    struct sigaction sa_usr1;

    // initializing strucuter 
    memset(&sa_term, 0, sizeof(sa_term));
    memset(&sa_usr1, 0, sizeof(sa_usr1));

    // Setup  SIGINT / SIGTERM
    sa_term.sa_handler = handle_termination_signal;     // Assing the function to handler
    sigemptyset(&sa_term.sa_mask);                      // init empty the signal mask
    sigaddset(&sa_term.sa_mask, SIGINT);                // Block SIGINT
    sigaddset(&sa_term.sa_mask, SIGTERM);               // Block SIGTERM
    sa_term.sa_flags = 0;                               // No-auto restart

    // Error handling in Setup
    if(sigaction(SIGINT, &sa_term, NULL) == -1) {
        perror("sigaction(SIGINT) failed");
        return -1;
    }

    if(sigaction(SIGTERM, &sa_term, NULL) == -1) {
        perror("sigaction(SIGTERM) failed");
        return -1;
    }

    // Setup SIGUSR1
    sa_usr1.sa_handler = handle_usr1_signal;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;

    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        perror("sigaction(SIGUSR1) failed");
        return -1;
    }
    
    return 0;
}