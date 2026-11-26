// Work for Student 2

#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>

typedef struct {
    pid_t pid;
    int restart_count;
    int is_running;
} ChildProcess;

int init_and_start_children(int count);

const ChildProcess *get_child_processes(int *count);
void handle_terminated_children(void);
void shutdown_all_children(int timeout_seconds);

#endif