#!/bin/bash
# Test scenarios for the watchdog project (Section 7)
# Run from anywhere:  bash tests/test_scenarios.sh   or   make test
#
# Workers: "service" runs until it is killed, "task" exits normally after 3 sec.
# Output of every run is saved under tests/logs/

cd "$(dirname "$0")/.." || exit 1

LOG=watchdog.log
LOG_DIR=tests/logs
RESULT_FILE=tests/test_results.txt
mkdir -p "$LOG_DIR"

# print to the screen and to the results file
exec > >(tee "$RESULT_FILE") 2>&1

PASS=0
FAIL=0
WD=""

check() {
    local desc="$1"
    shift
    if "$@"; then
        echo "  [PASS] $desc"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $desc"
        FAIL=$((FAIL + 1))
    fi
}

# true if the process exists and is not a zombie
is_alive() {
    local st
    st=$(ps -o stat= -p "$1" 2>/dev/null | tr -d ' ')
    [ -n "$st" ] && [[ "$st" != Z* ]]
}

# pid of the worker with the given mode (service / task), empty if none
get_child() {
    pgrep -P "$WD" -f "worker $1" | head -n 1
}

# checks that the pid really belongs to a child of our watchdog
is_our_child() {
    [ -n "$1" ] && [ "$(ps -o ppid= -p "$1" 2>/dev/null | tr -d ' ')" = "$WD" ]
}

# start watchdog in its own process group so we can send SIGINT
# to the whole group like Ctrl+C does in a terminal.
# stdbuf -oL: line buffered stdout, otherwise printf output stays in the buffer
start_watchdog() {
    rm -f "$LOG"
    setsid stdbuf -oL ./watchdog > "$1" 2>&1 &
    WD=$!
    sleep 1
}

# wait until watchdog exits, max $1 seconds
wait_exit() {
    for ((i = 0; i < $1 * 10; i++)); do
        if ! is_alive "$WD"; then
            wait "$WD" 2>/dev/null
            return 0
        fi
        sleep 0.1
    done
    return 1
}

# Safe cleanup
cleanup() {
    # Safe stop for watchdog
    if [ -n "$WD" ] && [ "$WD" -gt 1 ] 2>/dev/null; then
        if is_our_child "$WD"; then
            kill -KILL "$WD" 2>/dev/null
        fi
    fi

    # Safely stop only OUR worker processes
    pkill -KILL -u "$(id -u)" -P "$$" -x worker 2>/dev/null
}
trap cleanup EXIT

echo "Watchdog test scenarios - $(date '+%Y-%m-%d %H:%M:%S')"

# ------------------------------------------------------------------
echo
echo "== 1. Build =="
make clean > /dev/null
BUILD=$(make 2>&1)
check "make finished without error" [ $? -eq 0 ]
check "watchdog executable created" [ -x ./watchdog ]
check "worker executable created" [ -x ./worker ]
check "no compiler warnings" bash -c "! grep -qi warning <<< \"\$1\"" _ "$BUILD"
if [ ! -x ./watchdog ]; then
    echo "build failed, stopping"
    exit 1
fi

# ------------------------------------------------------------------
echo
echo "== 2. Normal operation =="
start_watchdog "$LOG_DIR/run1_output.txt"
SVC=$(get_child service)
TASK=$(get_child task)
echo "  watchdog PID: $WD, service PID: $SVC, task PID: $TASK"
check "watchdog is running" is_alive "$WD"
check "at least 2 child processes started" [ "$(pgrep -P "$WD" | wc -l)" -ge 2 ]
check "service worker is a child of watchdog" is_our_child "$SVC"
check "task worker is a child of watchdog" is_our_child "$TASK"
check "STARTUP written to log" grep -q "\[STARTUP\]" "$LOG"

# ------------------------------------------------------------------
echo
echo "== 3. Status report (SIGUSR1) =="
kill -USR1 "$WD"
sleep 1
grep "\[STATUS\]" "$LOG" | sed 's/^/    /'
check "status report written to log" grep -q "\[STATUS\] Status report" "$LOG"
check "service PID $SVC is in the report as RUNNING" grep -q "PID: $SVC, state: RUNNING" "$LOG"
check "task PID $TASK is in the report as RUNNING" grep -q "PID: $TASK, state: RUNNING" "$LOG"

# ------------------------------------------------------------------
echo
echo "== 4. Normal termination (task exits with code 0) =="
sleep 3
check "task process finished" bash -c "! ps -p $TASK > /dev/null"
check "exit code 0 detected" grep -q "Worker $TASK exited with code 0" "$LOG_DIR/run1_output.txt" "$LOG"
check "task was NOT restarted" [ -z "$(get_child task)" ]

# ------------------------------------------------------------------
echo
echo "== 5. Unexpected termination (kill -9 to service) =="
# Verify the child
if is_our_child "$SVC"; then
    check "PID $SVC verified before sending the signal"
    kill -KILL "$SVC"
    sleep 2
else
    echo "Refusing to kill PID $SVC: not our child process" >&2
fi
NEW_SVC=$(get_child service)
echo "  old PID: $SVC, new PID: $NEW_SVC"
check "termination by signal detected" grep -q "Worker $SVC terminated by signal 9" "$LOG_DIR/run1_output.txt" "$LOG"
check "service restarted with a new PID" bash -c "[ -n '$NEW_SVC' ] && [ '$NEW_SVC' != '$SVC' ]"
check "restart attempt 1 of 3" grep -q "attempt 1 of 3" "$LOG_DIR/run1_output.txt" "$LOG"

# ------------------------------------------------------------------
echo
echo "== 6. Restart limit (max 3) =="
for n in 2 3 4; do
    P=$(get_child service)
    if is_our_child "$P"; then
        kill -KILL "$P"
        echo "  kill $n -> PID $P"
    fi
    sleep 2
done
check "limit reached message" grep -q "Restart limit reached for worker service" "$LOG_DIR/run1_output.txt" "$LOG"
check "exactly 3 restarts done" [ "$(cat "$LOG_DIR/run1_output.txt" "$LOG" | grep -c "Restarting worker service")" -eq 3 ]
check "no more service process running" [ -z "$(get_child service)" ]
check "watchdog still running" is_alive "$WD"

kill -USR1 "$WD"
sleep 1
check "report shows service as NOT RUNNING with 3 restarts" grep -q "Child 0 -> PID: -, state: NOT RUNNING, restarts: 3" "$LOG"

kill -TERM "$WD"
wait_exit 10
cp "$LOG" "$LOG_DIR/run1_watchdog.log"

# ------------------------------------------------------------------
echo
echo "== 7. Graceful shutdown (SIGTERM) =="
start_watchdog "$LOG_DIR/run2_output.txt"
CHILDREN=$(pgrep -P "$WD" | tr '\n' ' ')
echo "  watchdog PID: $WD, children: $CHILDREN"
kill -TERM "$WD"
check "watchdog exited" wait_exit 10
check "SHUTDOWN with SIGTERM in log" grep -q "\[SHUTDOWN\] SIGTERM" "$LOG"
LEFT=0
for p in $CHILDREN; do ps -p "$p" > /dev/null && LEFT=$((LEFT + 1)); done
check "all children terminated and reaped (no zombie)" [ "$LEFT" -eq 0 ]
check "no restart during shutdown" bash -c "! grep -q Restarting $LOG_DIR/run2_output.txt $LOG"
cp "$LOG" "$LOG_DIR/run2_watchdog.log"

# ------------------------------------------------------------------
echo
echo "== 8. Ctrl+C (SIGINT) =="
start_watchdog "$LOG_DIR/run3_output.txt"
CHILDREN=$(pgrep -P "$WD" | tr '\n' ' ')
echo "  watchdog PID: $WD, children: $CHILDREN"
# negative pid = send to the whole process group, same as Ctrl+C
kill -INT -- "-$WD"
check "watchdog exited" wait_exit 10
check "SHUTDOWN with SIGINT in log" grep -q "\[SHUTDOWN\] SIGINT" "$LOG"
LEFT=0
for p in $CHILDREN; do ps -p "$p" > /dev/null && LEFT=$((LEFT + 1)); done
check "all children terminated and reaped" [ "$LEFT" -eq 0 ]
check "no restart after Ctrl+C" bash -c "! grep -q Restarting $LOG_DIR/run3_output.txt $LOG"
cp "$LOG" "$LOG_DIR/run3_watchdog.log"

# ------------------------------------------------------------------
echo
echo "== 9. Log verification (run 1) =="
L="$LOG_DIR/run1_watchdog.log"
check "every line has [date time] [EVENT] format" bash -c "! grep -Ev '^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] \[[A-Z_]+\] ' $L"
check "startup event in log" grep -q "\[STARTUP\]" "$L"
check "status report in log" grep -q "\[STATUS\]" "$L"
check "shutdown event in log" grep -q "\[SHUTDOWN\]" "$L"
check "child exit event in log" grep -Eq "Worker [0-9]+ (exited with code|terminated by signal)" "$L"
check "restart event in log" grep -q "Restarting worker" "$L"
check "restart limit event in log" grep -q "Restart limit reached" "$L"

# ------------------------------------------------------------------
echo
echo "== Result =="
echo "  passed: $PASS, failed: $FAIL"
echo "  logs saved in $LOG_DIR/"
[ "$FAIL" -eq 0 ]
