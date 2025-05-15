#!/usr/bin/env bash
#SBATCH --job-name=cvrp_dg
#SBATCH -p hpg2-compute
#SBATCH --qos=yu.yang1-b
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32GB
#SBATCH -t 3-23:00:00
#SBATCH --array=0-99
#SBATCH --output=cvrp_out/%A_%a.out
#SBATCH --error=cvrp_err/%A_%a.err

module load gcc
module load gurobi/12.0.2

COMMANDS=(

)

eval ${COMMANDS[$SLURM_ARRAY_TASK_ID]}