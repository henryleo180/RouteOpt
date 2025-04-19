#!/bin/bash
# run_experiments.sh
# This script runs multiple CVRP experiments over VRP instances organized in subdirectories.
# The instance files are stored in the following subdirectories under "instance":
#   120_200, 150_200, 180_200, 200_200
# Each subfolder contains multiple .vrp files (e.g., CVRP_200_1.vrp, etc.).
#
# Each .vrp file contains a COMMENT line that includes the optimum value in a format like:
#   COMMENT : (Zhengzhong. ... aver_route_length = 6.0 opt = 31598.0)
#
# The u_value is computed as the optimal value plus one (i.e., u_value = opt + 1).
# (If you prefer to use a 1% increment, see the commented alternative below.)
#
# For each instance, three variants are executed:
#   1. Standard variant ('cvrp')
#   2. Root cut only variant ('cvrp_rootcutonly')
#   3. Robust child variant ('cvrp_robustchild')
#
# Output logs are stored in the "./logs" directory with filenames based on the instance basename.

# Create logs directory if it doesn't exist.
mkdir -p ./logs

# Count total runs (3 experiments per file across all subdirectories).
total_runs=0
for folder in instance2/120_200 instance2/150_200 instance2/180_200 instance2/200_200; do
    for file in "$folder"/*.vrp; do
       total_runs=$((total_runs+1))
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

# Initialize run counter.
run_count=0

# Loop over each VRP instance file from the subfolders.
for folder in instance2/120_200 instance2/150_200 instance2/180_200 instance2/200_200; do
    for file in "$folder"/*.vrp; do
        # Extract the optimal value from the COMMENT line.
        # This line is expected to contain a substring like "opt = 31598.0".
        opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
        if [ -z "$opt" ]; then
            echo "Warning: opt value not found in $file. Skipping this file."
            continue
        fi

        # Compute u_value as optimal value plus 1.
        # We use awk for proper floating point arithmetic and rounding to an integer.
        u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')

        # Alternatively, if you would like to use 1% more than opt, uncomment the line below and comment out the above line.
        # u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt * 1.01 }')

        # Get the basename of the file (without the .vrp extension).
        base=$(basename "$file" .vrp)
        
        # Run the three experiment variants.
        
        # 1. Standard variant using 'cvrp'
        run_count=$((run_count+1))
        show_progress "$run_count"
        ./asaved_exe/cvrp_3b "$file" -u "$u" > "./logs_3b/output3b_${base}.txt" 2>&1
        
        # 2. Root cut only variant using 'cvrp_rootcutonly'
        # run_count=$((run_count+1))
        # show_progress "$run_count"
        # ./asaved_exe/cvrp_rootcutonly "$file" -u "$u" > "./logs/output_${base}_onlyr.txt" 2>&1
        
        # 3. Robust child variant using 'cvrp_robustchild'
        # run_count=$((run_count+1))
        # show_progress "$run_count"
        # ./asaved_exe/cvrp_robustchild "$file" -u "$u" > "./logs/output_${base}_robustc.txt" 2>&1

    done
done

# Final progress update.
show_progress "$total_runs"
echo -e "\nAll runs completed."
