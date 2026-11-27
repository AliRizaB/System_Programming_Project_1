#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "signal_manager.h"
#include "process_manager.h"
#include "logger.h"

#define INITIAL_WORKER_COUNT 2 // at least 2 child
#define SHUTDOWN_TIMEOUT_SEC 5 // Grace period before SIGKILL
#define LOG_FILE_PATH "watchdog.log"

int main(void) {
    // init logger
    if(init_logger(LOG_FILE_PATH) != 0) {
        fprintf(stderr, "Failed to initialize logger.\n");
        return EXIT_FAILURE;
    }

    // init signal handling
    if(init_signal_handlers() != 0) {
        fprintf(stderr, "Failed to initialize signal handlers.\n");
        close_logger();
        return EXIT_FAILURE;
    }

    log_event("STARTUP", "Watchdog supervisor started.");
    printf("Watchdog project started (PID: %d). Press Ctrl+C or send SIGTERM(kill -15) to stop.\n", getpid());

    // init child worker process 
    if(init_and_start_children(INITIAL_WORKER_COUNT) != 0) {
        log_event("ERROR", "Failed to start initial child processes.");
        close_logger();
        return EXIT_FAILURE;
    }

    // Main Supervise Control Loop
    while(!g_shutdown_requested){
        // Status Report SIGUSR1 kill -10 
        if(g_status_report_requested) {
            g_status_report_requested = 0; // Set the flag so future SIGUSR1s work.
            int child_count = 0;
            const ChildProcess *children = get_child_processes(&child_count);
            log_status_report(children, child_count);
            printf("[INFO] SIGUSR1 received: status report written to %s.\n", LOG_FILE_PATH);
        }
        
        // Terminate children
        handle_terminated_children();

        if(!g_shutdown_requested) {
            sleep(1);
        }
    }

    // Graceful Shutdown
    const char *sig_name = (g_shutdown_signal == SIGINT) ? "SIGINT (Ctrl+C)" : "SIGTERM";
    log_event("SHUTDOWN", sig_name);
    printf("\nReceived %s. Initiating gracefull shutdown...\n", sig_name);

    //Stop children
    shutdown_all_children(SHUTDOWN_TIMEOUT_SEC);
    log_event("SHUTDOWN", "All child process terminated successfully. Watchdog exiting gracefully.");
    close_logger();

    return EXIT_SUCCESS;
}
