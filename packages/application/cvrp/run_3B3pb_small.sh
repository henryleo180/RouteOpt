#!/bin/bash
# run_experiments.sh
# This script runs multiple CVRP experiments over VRP instances organized in subdirectories.
# Instances live under /home/haoran/data_eda/instance2/{120_200,150_200,180_200,200_200}
# Logs for the 3b variant go to /home/haoran/data_eda/3b_on_2bsetting

INSTANCE_ROOT="/home/haoran/data_eda/instance2"
LOG_ROOT="/home/haoran/data_eda/3b_3pb_exclude_exist"

mkdir -p "$LOG_ROOT"

# Create log directory if it doesn't exist
mkdir -p "$LOG_ROOT"

# Count total runs (1 experiment per file for the 3b variant)
total_runs=0
for sub in 120_200 150_200 180_200 200_200; do
    for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
        (( total_runs++ ))
    done
done

echo "Starting experiments: Total runs = $total_runs"

# Simple progress bar
show_progress() {
    local current=$1
    local percent=$(( (current * 100) / total_runs ))
    local bar_length=30
    local filled_length=$(( (bar_length * current) / total_runs ))
    local bar=$(printf "%${filled_length}s" | tr ' ' '#')
    local empty=$(printf "%$((bar_length - filled_length))s")
    echo -ne "\rProgress: [${bar}${empty}] ${percent}% (${current}/${total_runs})"
}

run_count=0

# Loop over each instance and run the 3b variant
for sub in 120_200 150_200 180_200 200_200; do
    for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
        # Extract the optimal value ("opt = 31598.0")
        opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
        if [ -z "$opt" ]; then
            echo "Warning: opt not found in $file; skipping."
            continue
        fi

        # Compute u_value = opt + 1
        u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')

        base=$(basename "$file" .vrp)

        # Run the 3b variant
        (( run_count++ ))
        show_progress "$run_count"
        ./bin/cvrp "$file" -u "$u" > "$LOG_ROOT/output_3b3pb_${base}.txt" 2>&1
    done
done