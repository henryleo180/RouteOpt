#!/bin/bash
# launch_2bon3bsetting.sh
# Starts run_exp_2bon3bsetting.sh under nohup, logs output, and saves the PID.

LOGDIR="/home/haoran/data_eda/3b_3pb_v5_only_ex1/nohup"
mkdir -p "$LOGDIR"

nohup bash run_3B3pb_small.sh \
    > "$LOGDIR/run_exp.out" 2>&1 &

# save its PID so you can monitor or kill later
echo $! > "$LOGDIR/run_exp.pid"

echo "Launched PID $(cat $LOGDIR/run_exp.pid), logging to $LOGDIR/run_exp.out"