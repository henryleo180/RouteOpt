#!/bin/bash
# launch_dual_gain_exp.sh
# Starts run_dual_gain_exp.sh under nohup, logs output, and saves the PID.

LOGDIR="/home/haoran/data_eda/dual_gain/nohup"
mkdir -p "$LOGDIR"

# The name of your script file
SCRIPT_NAME="run_dual_gain.sh"

nohup sh "$SCRIPT_NAME" \
    > "$LOGDIR/run_exp.out" 2>&1 &

# save its PID so you can monitor or kill later
echo $! > "$LOGDIR/run_exp.pid"
echo "Launched PID $(cat $LOGDIR/run_exp.pid), logging to $LOGDIR/run_exp.out"
echo "Monitor progress with: tail -f $LOGDIR/run_exp.out"
echo "Kill process if needed with: kill -9 $(cat $LOGDIR/run_exp.pid)"