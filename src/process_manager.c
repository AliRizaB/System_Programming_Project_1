#define _POSIX_C_SOURCE 200809L

#include "process_manager.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include "signal_manager.h"
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include "logger.h"

#define WORKER_COUNT 2
#define MAX_RESTARTS 3

static const char *const modes[WORKER_COUNT] = {"service", "task"};

static ChildProcess children[WORKER_COUNT];
static int child_count = 0;

static volatile sig_atomic_t child_check_requested = 0;

static void request_child_check(int signum)
{
    (void)signum;
    child_check_requested = 1;
}

static int init_child_monitoring(void)
{
    struct sigaction action = {0};
    action.sa_handler = request_child_check;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &action, NULL) == -1)
    {
        perror("sigaction SIGCHLD");
        return -1;
    }

    action.sa_flags = 0;
    if (sigaction(SIGALRM, &action, NULL) == -1)
    {
        perror("sigaction SIGALRM");
        return -1;
    }

    struct itimerval timer = {0};
    timer.it_value.tv_sec = 1;
    timer.it_interval.tv_sec = 1;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1)
    {
        perror("setitimer");
        return -1;
    }

    return 0;
}

static int start_child(ChildProcess *child, const char *mode)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        execl("./worker", "worker", mode, (char *)NULL);
        perror("execl ./worker");
        _exit(127);
    }

    child->pid = pid;
    child->is_running = 1;

    char buf[128];

    // Log child event;
    if (child->restart_count == 0)
    {

        printf("[%s] started (PID: %ld)\n", mode, (long)pid);
        snprintf(buf, sizeof(buf), "Child [%s] is created (PID: %ld)", mode, (long)pid);
        log_event("CHILD_CREATED", buf);
    }
    else
    {
        printf("[%s] re-started (PID: %ld, attempt %d/%d)\n",
               mode, (long)pid, child->restart_count, MAX_RESTARTS);
    }
    return 0;
}

int init_and_start_children(int count)
{
    if (count != WORKER_COUNT || child_count != 0)
    {
        return -1;
    }

    if (init_child_monitoring() != 0)
    {
        return -1;
    }

    const char *modes[WORKER_COUNT] = {"service", "task"};

    for (int i = 0; i < count; i++)
    {
        if (start_child(&children[i], modes[i]) != 0)
        {
            shutdown_all_children(5);
            return -1;
        }

        child_count++;
    }

    return 0;
}

const ChildProcess *get_child_processes(int *count)
{
    if (count != NULL)
    {
        *count = child_count;
    }

    return children;
}

void handle_terminated_children(void)
{
    if (!child_check_requested)
    {
        return;
    }

    child_check_requested = 0;
    for (;;)
    {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0)
        {
            return;
        }

        if (pid < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno != ECHILD)
            {
                perror("waitpid");
            }
            return;
        }

        for (int i = 0; i < child_count; i++)
        {
            if (children[i].pid != pid)
            {
                continue;
            }

            children[i].is_running = 0;
            children[i].pid = 0;
            char msg[256];

            if (WIFEXITED(status))
            {
                snprintf(msg, sizeof msg, "Worker %ld exited with code %d", (long)pid, WEXITSTATUS(status));
                log_event("EXIT", msg);
            }
            else if (WIFSIGNALED(status))
            {
                snprintf(msg, sizeof msg, "Worker %ld terminated by signal %d", (long)pid, WTERMSIG(status));
                log_event("EXIT", msg);
            }
            int completed = WIFEXITED(status) && WEXITSTATUS(status) == 0;

            if (!completed && !g_shutdown_requested)
            {
                if (children[i].restart_count < MAX_RESTARTS)
                {
                    children[i].restart_count++;

                    snprintf(msg, sizeof msg, "Restarting worker %s (attempt %d of %d)",
                             modes[i], children[i].restart_count, MAX_RESTARTS);
                    log_event("CHILD_RESTART", msg);

                    if (start_child(&children[i], modes[i]) != 0)
                    {
                        fprintf(stderr, "Failed to restart worker %s\n", modes[i]);
                    }
                }
                else
                {
                    snprintf(msg, sizeof msg, "Restart limit reached for worker %s", modes[i]);
                    log_event("RESTART", msg);
                }
            }
            break;
        }
    }
}

static int reap_on_shutdown(ChildProcess *child, int options)
{
    pid_t result;

    do
    {
        result = waitpid(child->pid, NULL, options);
    } while (result == -1 && errno == EINTR);

    if (result == 0)
    {
        return 0;
    }

    if (result == -1 && errno != ECHILD)
    {
        perror("waitpid during shutdown");
        return -1;
    }

    child->is_running = 0;
    child->pid = 0;
    return 1;
}

void shutdown_all_children(int timeout_seconds)
{
    struct itimerval stopped_timer = {0};
    if (setitimer(ITIMER_REAL, &stopped_timer, NULL) == -1)
    {
        perror("stop timer");
    }

    for (int i = 0; i < child_count; i++)
    {
        if (children[i].is_running && children[i].pid > 0)
        {
            if (kill(children[i].pid, SIGTERM) == -1 && errno != ESRCH)
            {
                perror("send SIGTERM");
            }
        }
    }

    struct timespec deadline;
    int have_deadline = clock_gettime(CLOCK_MONOTONIC, &deadline) == 0;
    if (have_deadline)
    {
        deadline.tv_sec += timeout_seconds;
    }
    else
    {
        perror("clock_gettime");
    }

    while (have_deadline)
    {
        int still_running = 0;

        for (int i = 0; i < child_count; i++)
        {
            if (children[i].is_running)
            {
                if (reap_on_shutdown(&children[i], WNOHANG) != 1)
                {
                    still_running = 1;
                }
            }
        }

        if (!still_running)
        {
            return;
        }

        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
        {
            perror("clock_gettime");
            break;
        }

        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
        {
            break;
        }

        struct timespec pause_time = {0, 100000000L};
        nanosleep(&pause_time, NULL);
    }

    for (int i = 0; i < child_count; i++)
    {
        if (!children[i].is_running || children[i].pid <= 0)
        {
            continue;
        }

        if (kill(children[i].pid, SIGKILL) == -1 && errno != ESRCH)
        {
            perror("send SIGKILL");
            continue;
        }

        reap_on_shutdown(&children[i], 0);
    }
}