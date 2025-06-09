#!/bin/bash
# generate_all_instances_slurm.sh
# This script generates CVRP SLURM job script for ALL instances in all 4 folders

INSTANCE_ROOT="/blue/yu.yang1/haoran.liu/instance2"

# Define the 4 folders
FOLDERS=(
    "120_200"
    # "150_200"
    # "180_200"
    # "200_200"
)

# Create output directories if they don't exist
mkdir -p cvrp_out cvrp_err

# Generate the complete SLURM job script
generate_slurm_script() {
    local output_file="$1"
    
    # Start the SLURM script
    cat > "$output_file" << 'EOF_HEADER'
#!/usr/bin/env bash
#SBATCH --job-name=cvrp_all
#SBATCH -p hpg2-compute
#SBATCH --qos=yu.yang1-b
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32GB
#SBATCH -t 2:00:00
#SBATCH --array=PLACEHOLDER
#SBATCH --output=cvrp_out/%A_%a.out
#SBATCH --error=cvrp_err/%A_%a.err

module load gcc
module load gurobi/11.0.1

COMMANDS=(
EOF_HEADER

    # Generate commands for all instances
    local cmd_count=0
    for folder in "${FOLDERS[@]}"; do
        echo "Processing folder: $folder" >&2
        
        # Process all .vrp files in the current folder
        for file in "$INSTANCE_ROOT/$folder"/*.vrp; do
            # Check if file exists (handles case where no .vrp files exist)
            if [ ! -f "$file" ]; then
                continue
            fi
            
            # Extract the optimal value
            opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
            if [ -z "$opt" ]; then
                echo "Warning: opt not found in $file; skipping." >&2
                continue
            fi
            
            # Compute u_value = opt + 1
            u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')
            
            # Extract just the filename for cleaner output
            filename=$(basename "$file")
            instance_name="${filename%.vrp}"
            
            # Output the command
            echo "    \"./bin/cvrp $file -u $u\"  # Instance: $instance_name" >> "$output_file"
            ((cmd_count++))
        done
    done
    
    # Close the COMMANDS array and add the execution line
    cat >> "$output_file" << 'EOF_FOOTER'
)

eval ${COMMANDS[$SLURM_ARRAY_TASK_ID]}
EOF_FOOTER

    # Update the array size in the SLURM script
    local array_end=$((cmd_count - 1))
    sed -i "s/#SBATCH --array=PLACEHOLDER/#SBATCH --array=0-${array_end}/" "$output_file"
    
    echo "Generated SLURM script with $cmd_count commands (array 0-${array_end})"
    return $cmd_count
}

# Generate the SLURM job script
SLURM_FILE="slurm_job_all_instances.sh"
echo "Generating complete SLURM job script for all instances..."
generate_slurm_script "$SLURM_FILE"
cmd_total=$?

# Make it executable
chmod +x "$SLURM_FILE"

echo ""
echo "Generated complete SLURM job script: $SLURM_FILE"
echo "- Total commands: $cmd_total"
echo "- Expected instances: ~800 (200 per folder × 4 folders)"
echo ""
echo "To submit: sbatch $SLURM_FILE"

# Optional: Show breakdown by folder
echo ""
echo "Breakdown by folder:"
for folder in "${FOLDERS[@]}"; do
    count=$(grep -c "$folder/" "$SLURM_FILE" 2>/dev/null || echo "0")
    echo "- $folder: $count instances"
done