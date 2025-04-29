#!/bin/bash
# run_experiments.sh with checkpoint resume functionality

# Create logs directory if it doesn't exist.
mkdir -p ./logs

# Define the checkpoint file.
checkpoint_file="resume.chk"

# Read the current checkpoint; if not present, start at 0.
if [ -f "$checkpoint_file" ]; then
    resume_index=$(cat "$checkpoint_file")
    echo "Resuming from run number $resume_index"
else
    resume_index=0
fi

# Count total runs (3 experiments per file across all subdirectories).
total_runs=0
for folder in instance2/180_200 instance2/200_200; do
    for file in "$folder"/*.vrp; do
       total_runs=$((total_runs+3))
    done
done

echo "Starting experiments: Total runs = $total_runs"

# Function to display a simple progress bar.
show_progress() {
    current=$1
    percent=$(( (current * 100) / total_runs ))
    bar_length=30
    filled_length=$(( (bar_length * current) / total_runs ))
    bar=$(printf "%${filled_length}s" | tr ' ' '#')
    empty=$(printf "%$((bar_length - filled_length))s")
    echo -ne "\rProgress: [${bar}${empty}] ${percent}% (${current}/${total_runs})"
}

# Initialize the run counter.
run_count=0

# Loop over each VRP instance file from the subfolders.
for folder in instance2/180_200 instance2/200_200; do
    for file in "$folder"/*.vrp; do
        # Extract the optimal value from the COMMENT line.
        opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
        if [ -z "$opt" ]; then
            echo "Warning: opt value not found in $file. Skipping this file."
            continue
        fi

        # Compute u_value as optimal value plus 1.
        u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')

        # Get the basename of the file (without the .vrp extension).
        base=$(basename "$file" .vrp)

        # 1. Standard variant using 'cvrp'
        run_count=$((run_count+1))
        if [ "$run_count" -le "$resume_index" ]; then
            echo -e "\nSkipping run $run_count (standard variant for ${base}) as already completed"
        else
            show_progress "$run_count"
            ./bin_saved/cvrp "$file" -u "$u" > "./logs/output_${base}.txt" 2>&1
            echo "$run_count" > "$checkpoint_file"
        fi

        # 2. Root cut only variant using 'cvrp_rootcutonly'
        run_count=$((run_count+1))
        if [ "$run_count" -le "$resume_index" ]; then
            echo -e "\nSkipping run $run_count (root cut only variant for ${base}) as already completed"
        else
            show_progress "$run_count"
            ./bin_saved/cvrp_rootcutonly "$file" -u "$u" > "./logs/output_${base}_onlyr.txt" 2>&1
            echo "$run_count" > "$checkpoint_file"
        fi

        # 3. Robust child variant using 'cvrp_robustchild'
        run_count=$((run_count+1))
        if [ "$run_count" -le "$resume_index" ]; then
            echo -e "\nSkipping run $run_count (robust child variant for ${base}) as already completed"
        else
            show_progress "$run_count"
            ./bin_saved/cvrp_robustchild "$file" -u "$u" > "./logs/output_${base}_robustc.txt" 2>&1
            echo "$run_count" > "$checkpoint_file"
        fi

    done
done

show_progress "$total_runs"
echo -e "\nAll runs completed."
