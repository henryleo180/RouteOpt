#!/bin/bash

# Build section (unchanged)
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd ..

# Configuration
INSTANCE_ROOT="/home/haoran/data_eda/instance2"
LOG_ROOT="/home/haoran/data_eda/2b_wo_new"
BIN_STORE="/home/haoran/data_eda/bin_store"

# Simple resource management
TOTAL_CORES=$(nproc)
echo "System has $TOTAL_CORES CPU cores"

# Start with conservative parallelism, then scale up
MAX_JOBS=8  # Start with 8 jobs - safe for your 125GB system
echo "Using $MAX_JOBS parallel jobs"

# Create directories
mkdir -p "$LOG_ROOT"
mkdir -p "$BIN_STORE"

# Create timestamped executable
timestamp=$(date +"%Y%m%d_%H%M%S")
cvrp_copy="$BIN_STORE/cvrp_${timestamp}"
cp ./bin/cvrp "$cvrp_copy"
chmod +x "$cvrp_copy"

echo "Created executable copy: $cvrp_copy"

# Simple function to wait for job slots
wait_for_slot() {
    while [ $(jobs -r | wc -l) -ge $MAX_JOBS ]; do
        sleep 0.5
    done
}

# Function to run single experiment
run_single_experiment() {
    local file="$1"
    local job_id="$2"
    
    local base=$(basename "$file" .vrp)
    echo "Starting job $job_id: $base"
    
    # Extract optimal value
    local opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
    if [ -z "$opt" ]; then
        echo "Warning: opt not found in $file; skipping."
        return 1
    fi

    # Compute u_value = opt + 1
    local u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')
    
    # Run experiment
    "$cvrp_copy" "$file" -u "$u" > "$LOG_ROOT/output_2bwo_${base}.txt" 2>&1
    local exit_code=$?
    
    echo "Completed job $job_id: $base (exit code: $exit_code)"
    return $exit_code
}

# Collect all files to process
files_array=()
for sub in 180_200 200_200; do
    for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
        if [ -f "$file" ]; then
            files_array+=("$file")
        fi
    done
done

total_files=${#files_array[@]}
echo "Total experiments to run: $total_files"

if [ $total_files -eq 0 ]; then
    echo "No .vrp files found!"
    exit 1
fi

# Progress reporting in background
progress_reporter() {
    while true; do
        sleep 30
        local running_jobs=$(jobs -r | wc -l)
        local completed_files=$(find "$LOG_ROOT" -name "output_2bwo_*.txt" | wc -l)
        echo "Progress: $completed_files/$total_files completed, $running_jobs jobs running"
    done
} &
reporter_pid=$!

# Process all files
job_counter=1
start_time=$(date +%s)

echo "Starting parallel processing..."
for file in "${files_array[@]}"; do
    # Wait for available slot
    wait_for_slot
    
    # Start job in background
    run_single_experiment "$file" "$job_counter" &
    
    ((job_counter++))
    
    # Brief pause
    sleep 0.1
done

# Kill progress reporter
kill $reporter_pid 2>/dev/null

# Wait for all jobs to complete
echo "Waiting for all jobs to complete..."
wait

end_time=$(date +%s)
duration=$((end_time - start_time))

# Final statistics
completed_files=$(find "$LOG_ROOT" -name "output_2bwo_*.txt" | wc -l)
echo ""
echo "=== COMPLETION SUMMARY ==="
echo "Completed: $completed_files/$total_files experiments"
echo "Total time: ${duration} seconds"
if [ $total_files -gt 0 ]; then
    echo "Average time per experiment: $(( duration / total_files )) seconds"
fi
echo "Executable used: $cvrp_copy"
uptime

# Optional: Show system resource usage
echo ""
echo "Final system status:"
free -h | head -2