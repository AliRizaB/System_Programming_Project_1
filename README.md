# System Programming Project 1 - Watchdog

Sakarya University, Software Engineering - System Programming 2026-2027 Fall, Term Project 1

POSIX signal based process monitoring and management system written in C.
The main program (`watchdog`) starts child processes, watches them, restarts the ones
that crash (max 3 times) and shuts everything down cleanly when it gets SIGINT or SIGTERM.

## Requirements

- Linux (we used Ubuntu on WSL2)
- gcc, make
- for the test script: bash, ps, pgrep, setsid, stdbuf (all come with Ubuntu)

## Build

```bash
make          # builds ./watchdog and ./worker
make clean    # removes the executables and watchdog.log
```

## Run

```bash
./watchdog
```

The watchdog prints its PID when it starts. All events are written to `watchdog.log`
in the same folder.

From another terminal:

```bash
pgrep -a -P <WATCHDOG_PID>        # list the workers (check the PID before killing!)

kill -SIGUSR1 <WATCHDOG_PID>      # writes PID and state of every child to the log
kill -SIGKILL <WORKER_PID>        # crash a worker -> watchdog restarts it
kill -SIGTERM <WATCHDOG_PID>      # graceful shutdown
```

Ctrl+C in the watchdog terminal also starts the graceful shutdown.

```bash
tail -f watchdog.log              # follow the log
```

### Workers

| Mode | What it does |
|---|---|
| `service` | runs until it gets a signal |
| `task` | sleeps 3 seconds and exits with code 0 (normal termination, not restarted) |

## Log format

Every line is `[date time] [EVENT_TYPE] message`, for example:

```
[2026-11-26 22:58:14] [STARTUP] Watchdog supervisor started.
[2026-11-26 22:58:15] [STATUS] Status report (SIGUSR1) - watchdog PID: 189, children: 2, running: 2
[2026-11-26 22:58:15] [STATUS] Child 0 -> PID: 191, state: RUNNING, restarts: 0
[2026-11-26 22:58:15] [STATUS] Child 1 -> PID: 192, state: RUNNING, restarts: 0
[2026-11-26 22:58:26] [SHUTDOWN] SIGTERM
```

The log file is opened with `O_APPEND | O_CLOEXEC` and written with `write()`.
Logging is only done from the main loop, never inside a signal handler.

## Tests

```bash
make test     # or: bash tests/test_scenarios.sh
```

The script builds the project and runs all the scenarios from Section 7 automatically
(normal operation, normal termination, unexpected termination, restart limit,
SIGUSR1 report, SIGTERM, Ctrl+C, log check). Details are in
[docs/TEST_SCENARIOS.md](docs/TEST_SCENARIOS.md).

Results go to `tests/test_results.txt` and the logs of each run to `tests/logs/`.

## Project structure

```
├── Makefile
├── README.md
├── include/
│   ├── signal_manager.h
│   ├── process_manager.h
│   └── logger.h
├── src/
│   ├── main.c
│   ├── signal_manager.c
│   ├── process_manager.c
│   └── logger.c
├── workers/
│   └── worker.c
├── tests/
│   └── test_scenarios.sh
└── docs/
    └── TEST_SCENARIOS.md
```

## Team

| Student | GitHub | Module |
|---|---|---|
| Student 1 - Ali Rıza | AliRizaB | Signal management and main program (`signal_manager.*`, `main.c`) |
| Student 2 - Beyzanur | byznrblzr | Child process management (`process_manager.*`, `worker.c`) |
| Student 3 - İsmail Dal | ismaildal8240-sys | Logging and tests (`logger.*`, `Makefile`, `test_scenarios.sh`, `README.md`) |
