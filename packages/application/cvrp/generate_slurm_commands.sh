#!/bin/bash
# generate_specific_slurm_commands.sh
# This script generates CVRP commands for SLURM based on specific instance list

INSTANCE_ROOT="/blue/yu.yang1/haoran.liu/instance2"

# Define instances from your list (format: folder_number)
INSTANCES=(
    "180_169"
    "180_178"
    "180_133"
    "180_107"
    "180_144"
    "180_180"
    "150_31"
    "180_153"
    "180_188"
    "150_165"
    "150_27"
    "150_33"
    "180_131"
    "180_15"
    "180_172"
    "180_19"
    "150_138"
    "150_198"
    "180_111"
    "180_120"
    "180_135"
    "180_137"
    "180_149"
    "120_118"
    "120_139"
    "120_40"
    "120_84"
    "150_139"
    "200_83"
    "200_127"
    "200_56"
    "200_12"
    "200_194"
    "200_37" 
    "200_168"
    "200_91"
    "200_144"
    "200_62"
)

# Generate commands
generate_commands() {
    echo "COMMANDS=("
    
    for instance in "${INSTANCES[@]}"; do
        # Split the instance into folder and number
        folder="${instance%%_*}_200"  # Add _200 suffix to match folder structure
        number="${instance#*_}"
        
        # Find matching files
        for file in "$INSTANCE_ROOT/$folder"/*_${number}.vrp; do
            # Check if file exists
            if [ ! -f "$file" ]; then
                echo "# Warning: No instance with number ${number} found in ${folder}" >&2
                continue
            fi
            
            # Extract the optimal value
            opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" | grep -Eo "[0-9]+(\.[0-9]+)?")
            if [ -z "$opt" ]; then
                echo "# Warning: opt not found in $file; skipping." >&2
                continue
            fi
            
            # Compute u_value = opt + 1
            u=$(awk -v opt="$opt" 'BEGIN { printf "%.0f", opt + 1 }')
            
            # Output the command with proper quoting
            echo "    \"./bin/cvrp \\\"$file\\\" -u $u\"  # Instance: ${folder}_${number}"
        done
    done
    
    echo ")"
}

# Create commands file
COMMANDS_FILE="slurm_commands.sh"
generate_commands > $COMMANDS_FILE

# Count actual commands (not counting comments or empty lines)
CMD_COUNT=$(grep -c "^\s*\"./bin/cvrp" $COMMANDS_FILE)
ARRAY_END=$((CMD_COUNT - 1))

# Create SLURM job script
cat > slurm_job.sh << EOF
#!/usr/bin/env bash
#SBATCH --job-name=cvrp_dg
#SBATCH -p hpg2-compute
#SBATCH --qos=yu.yang1-b
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32GB
#SBATCH --array=0-${ARRAY_END}  # Automatically sized based on commands
#SBATCH -t 2:00:00
#SBATCH --output=cvrp_out/%A_%a.out
#SBATCH --error=cvrp_err/%A_%a.err

# Source the commands file
source ${COMMANDS_FILE}

# Run the command corresponding to this array job
eval \${COMMANDS[\$SLURM_ARRAY_TASK_ID]}
EOF

chmod +x slurm_job.sh

echo "Generated SLURM job script with array size 0-${ARRAY_END} (${CMD_COUNT} tasks)"
echo "To submit: sbatch slurm_job.sh"