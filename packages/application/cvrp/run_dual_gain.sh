#!/bin/bash
# run_experiments.sh
# This script runs 5 specific CVRP instances from each type of instances

INSTANCE_ROOT="/home/haoran/data_eda/instance2"
LOG_ROOT="/home/haoran/data_eda/dual_gain"
LOG_FILE="/home/haoran/solver/RouteOpt/packages/application/cvrp/dual_gain_log.txt"

# Create log directory if it doesn't exist
mkdir -p "$LOG_ROOT"

# Delete existing log file and create a new one with header
rm -f "$LOG_FILE"
echo "This is the dual gain log" > "$LOG_FILE"
echo "" >> "$LOG_FILE"

# Define the specific instance numbers to run
INSTANCE_NUMBERS=(1 100 101 111 110)

# Count total runs
total_runs=$(( ${#INSTANCE_NUMBERS[@]} * 4 ))  # 5 instances * 4 directories
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

# Loop over each instance type
for sub in 120_200 150_200 180_200 200_200; do
    # Loop over specific instance numbers
    for num in "${INSTANCE_NUMBERS[@]}"; do
        # Find files matching the specific number pattern
        for file in "$INSTANCE_ROOT/$sub"/*_${num}.vrp; do
            # Check if file exists
            if [ ! -f "$file" ]; then
                echo "Warning: No instance with number ${num} found in ${sub}. Skipping."
                continue
            fi
            
            # Extract the optimal value
            opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
            if [ -z "$opt" ]; then
                echo "Warning: opt not found in $file; skipping."
                continue
            fi
            
            # Compute u_value = opt + 1
            u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')
            base=$(basename "$file" .vrp)
            
            # Log the instance name to the log file
            echo "Instance Name: $base" >> "$LOG_FILE"
            
            # Run the solver
            (( run_count++ ))
            show_progress "$run_count"
            ./bin/cvrp "$file" -u "$u" > "$LOG_ROOT/output_${base}.txt" 2>&1
        done
    done
done

echo -e "\nExperiments completed."