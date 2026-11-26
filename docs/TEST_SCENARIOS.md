# Test Scenarios

All scenarios from Section 7 are in `tests/test_scenarios.sh`. Run it with:

```bash
make test
```

The script starts the watchdog in its own process group (`setsid`), sends real signals
to it and checks the process list (`ps`, `pgrep`) and the log file. Before killing a
worker, it always checks that the PID really is a child of our watchdog.

Each run is saved in `tests/logs/` (`runX_watchdog.log` = log file, `runX_output.txt` =
terminal output) and the summary is written to `tests/test_results.txt`.

| # | Scenario | What the script does | Expected result |
|---|---|---|---|
| 1 | Build | `make clean`, `make` | `watchdog` and `worker` are created, no warnings |
| 2 | Normal operation | start `./watchdog` | 2 children (`service`, `task`) running, their parent is the watchdog, `STARTUP` in log |
| 3 | Status report | `kill -SIGUSR1 <watchdog>` | report with the PID and state (`RUNNING`) of both children written to the log |
| 4 | Normal termination | wait for `task` (exits with 0 after 3 s) | exit code 0 detected, `task` is not restarted |
| 5 | Unexpected termination | check PID, `kill -SIGKILL <service>` | termination by signal 9 detected, `service` restarted with a new PID (attempt 1 of 3) |
| 6 | Restart limit | kill `service` 3 more times | restarts 2 and 3 happen, after the 4th kill "Restart limit reached", exactly 3 restarts, no new `service`, watchdog keeps running, SIGUSR1 report shows `NOT RUNNING, restarts: 3` |
| 7 | Graceful shutdown | `kill -SIGTERM <watchdog>` (new run) | watchdog exits, `[SHUTDOWN] SIGTERM` in log, all children terminated and reaped, no restart |
| 8 | Ctrl+C | `kill -INT -- -<pgid>` (SIGINT to the whole process group, same as Ctrl+C) | watchdog exits, `[SHUTDOWN] SIGINT` in log, all children reaped, no restart |
| 9 | Log verification | check the log of run 1 | every line has `[date time] [EVENT]` format; startup, status, shutdown, child exit, restart and restart limit events are all in the log |

## Manual test

```bash
# terminal 1
make
./watchdog

# terminal 2
pgrep -a -P <WATCHDOG_PID>       # find the worker PIDs
kill -SIGUSR1 <WATCHDOG_PID>
kill -SIGKILL <SERVICE_PID>
kill -SIGTERM <WATCHDOG_PID>     # or Ctrl+C in terminal 1
cat watchdog.log
```
