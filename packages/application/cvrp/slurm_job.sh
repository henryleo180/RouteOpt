#!/usr/bin/env bash
#SBATCH --job-name=cvrp_dg
#SBATCH -p hpg2-compute
#SBATCH --qos=yu.yang1-b
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32GB
#SBATCH --array=0-37  # Automatically sized based on commands
#SBATCH -t 2:00:00
#SBATCH --output=cvrp_out/%A_%a.out
#SBATCH --error=cvrp_err/%A_%a.err

# Source the commands file
source slurm_commands.sh

# Run the command corresponding to this array job
eval ${COMMANDS[$SLURM_ARRAY_TASK_ID]}
