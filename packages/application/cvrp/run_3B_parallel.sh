#!/bin/bash

# Build section (unchanged)
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd ..

# Configuration
INSTANCE_ROOT="/home/haoran/data_eda/instance2"
LOG_ROOT="/home/haoran/data_eda/3b_3pb_plus2b_v1"
BIN_STORE="/home/haoran/data_eda/bin_store"

# Maximum number of parallel jobs
MAX_JOBS=8
CURRENT_JOBS=0

# Create directories
mkdir -p "$LOG_ROOT"
mkdir -p "$BIN_STORE"

# Create timestamped executable
timestamp=$(date +"%Y%m%d_%H%M%S")
cvrp_copy="$BIN_STORE/cvrp_${timestamp}"
cp ./bin/cvrp "$cvrp_copy"
chmod +x "$cvrp_copy"

echo "Created executable copy: $cvrp_copy"
echo "Using $MAX_JOBS parallel jobs"

# Progress tracking with lock file
progress_file="/tmp/cvrp_progress_$$"
echo "0" > "$progress_file"

# Function to update progress (thread-safe)
update_progress() {
    local total="$1"
    (
        flock -x 200
        local current=$(cat "$progress_file")
        ((current++))
        echo "$current" > "$progress_file"
        
        local percent=$(( (current * 100) / total ))
        local bar_length=30
        local filled_length=$(( (bar_length * current) / total ))
        local bar=$(printf "%${filled_length}s" | tr ' ' '#')
        local empty=$(printf "%$((bar_length - filled_length))s")
        echo -e "\rProgress: [${bar}${empty}] ${percent}% (${current}/${total})" >&2
    ) 200>"$progress_file.lock"
}

# Function to wait for job slot
wait_for_slot() {
    while [ $CURRENT_JOBS -ge $MAX_JOBS ]; do
        # Wait for any background job to finish
        if wait -n 2>/dev/null; then
            ((CURRENT_JOBS--))
        else
            # Fallback for systems where wait -n doesn't work
            sleep 0.1
            # Count actual running jobs
            CURRENT_JOBS=$(jobs -r | wc -l)
        fi
    done
}

# Function to run single experiment in background
run_single_experiment() {
    local file="$1"
    local total="$2"
    
    # Extract optimal value
    local opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
    if [ -z "$opt" ]; then
        echo "Warning: opt not found in $file; skipping." >&2
        return 1
    fi

    # Compute u_value = opt + 1
    local u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')
    local base=$(basename "$file" .vrp)
    
    # Run experiment
    "$cvrp_copy" "$file" -u "$u" > "$LOG_ROOT/output_3b3pb_${base}.txt" 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        update_progress "$total"
    else
        echo "Error running experiment for $base (exit code: $exit_code)" >&2
    fi
}

# Count total files
total_files=0
for sub in 180_200 200_200; do
    for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
        if [ -f "$file" ]; then
            ((total_files++))
        fi
    done
done

echo "Total experiments to run: $total_files"

# Check if we have files to process
if [ $total_files -eq 0 ]; then
    echo "No .vrp files found in the specified directories!"
    exit 1
fi

# Process all files
start_time=$(date +%s)
for sub in 180_200 200_200; do
    for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
        if [ ! -f "$file" ]; then
            continue
        fi
        
        # Wait for available slot
        wait_for_slot
        
        # Start job in background
        run_single_experiment "$file" "$total_files" &
        
        ((CURRENT_JOBS++))
    done
done

# Wait for all remaining jobs to complete
echo -e "\nWaiting for all jobs to complete..."
wait

end_time=$(date +%s)
duration=$((end_time - start_time))

echo ""
echo "All experiments completed!"
echo "Executable used: $cvrp_copy"
echo "Total time: ${duration} seconds"
echo "Final system load:"
uptime

# Cleanup
rm -f "$progress_file" "$progress_file.lock"