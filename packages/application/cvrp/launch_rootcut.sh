#!/bin/bash
# launch_rootcut.sh
# Starts run_exp_rootcut.sh under nohup, saves the output log and PID in the current directory.

LOG_FILE="nohup_run_exp_rootcut.log"
PID_FILE="bohup_run_exp_rootcut.pid"

# Launch under nohup, capturing stdout+stderr
nohup sh run_exp_rootcut.sh > "$LOG_FILE" 2>&1 &

# Save the background process PID
echo $! > "$PID_FILE"

echo "Started run_exp_rootcut.sh (PID $(cat $PID_FILE))."
echo "Logging to $LOG_FILE"
