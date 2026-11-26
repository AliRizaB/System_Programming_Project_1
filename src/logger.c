#define _POSIX_C_SOURCE 200809L

#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

static int log_fd = -1;

// write() may write less bytes than we want or get interrupted by a signal,
// so keep writing until the whole buffer is done
static void write_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        buf += n;
        len -= (size_t)n;
    }
}

static void get_time_str(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm tm_now;

    if (localtime_r(&now, &tm_now) == NULL ||
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", &tm_now) == 0) {
        snprintf(buf, size, "unknown time");
    }
}

int init_logger(const char *log_file_path) {
    // O_APPEND -> every write goes to the end of the file
    // O_CLOEXEC -> worker processes dont inherit this fd after exec
    log_fd = open(log_file_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (log_fd == -1) {
        perror("open log file");
        return -1;
    }
    return 0;
}

// NOTE: not async-signal-safe (snprintf, localtime_r), only call it from
// the main loop, never inside a signal handler
void log_event(const char *event_type, const char *message) {
    char time_str[32];
    char line[512];
    int len;

    if (log_fd == -1)
        return;

    get_time_str(time_str, sizeof(time_str));
    len = snprintf(line, sizeof(line), "[%s] [%s] %s\n", time_str, event_type, message);
    if (len < 0)
        return;

    // message too long, cut it but keep the newline at the end
    if (len >= (int)sizeof(line)) {
        len = sizeof(line) - 1;
        line[len - 1] = '\n';
    }

    write_all(log_fd, line, (size_t)len);
}

void log_status_report(const ChildProcess *children, int count) {
    char msg[256];
    int running = 0;

    if (children == NULL)
        count = 0;

    for (int i = 0; i < count; i++) {
        if (children[i].is_running)
            running++;
    }

    snprintf(msg, sizeof(msg), "Status report (SIGUSR1) - watchdog PID: %d, children: %d, running: %d",
             (int)getpid(), count, running);
    log_event("STATUS", msg);

    for (int i = 0; i < count; i++) {
        if (children[i].is_running) {
            snprintf(msg, sizeof(msg), "Child %d -> PID: %d, state: RUNNING, restarts: %d",
                     i, (int)children[i].pid, children[i].restart_count);
        } else {
            snprintf(msg, sizeof(msg), "Child %d -> PID: -, state: NOT RUNNING, restarts: %d",
                     i, children[i].restart_count);
        }
        log_event("STATUS", msg);
    }
}

void close_logger(void) {
    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
    }
}
