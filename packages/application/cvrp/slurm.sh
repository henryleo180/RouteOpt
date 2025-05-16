#!/usr/bin/env bash
#SBATCH --job-name=cvrp_dg
#SBATCH -p hpg2-compute
#SBATCH --qos=yu.yang1-b
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=32GB
#SBATCH -t 2:00:00
#SBATCH --array=0-37 
#SBATCH --output=cvrp_out/%A_%a.out
#SBATCH --error=cvrp_err/%A_%a.err

module load gcc
module load gurobi/11.0.1

COMMANDS=(
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_169.vrp -u 29835"  # Instance: 180_200_169
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_178.vrp -u 27548"  # Instance: 180_200_178
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_133.vrp -u 26113"  # Instance: 180_200_133
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_107.vrp -u 27395"  # Instance: 180_200_107
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_144.vrp -u 26703"  # Instance: 180_200_144
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_180.vrp -u 29051"  # Instance: 180_200_180
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_31.vrp -u 20327"  # Instance: 150_200_31
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_153.vrp -u 26354"  # Instance: 180_200_153
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_188.vrp -u 28833"  # Instance: 180_200_188
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_165.vrp -u 23582"  # Instance: 150_200_165
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_27.vrp -u 23789"  # Instance: 150_200_27
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_33.vrp -u 25149"  # Instance: 150_200_33
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_131.vrp -u 26128"  # Instance: 180_200_131
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_15.vrp -u 24530"  # Instance: 180_200_15
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_172.vrp -u 23749"  # Instance: 180_200_172
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_19.vrp -u 29387"  # Instance: 180_200_19
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_138.vrp -u 21643"  # Instance: 150_200_138
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_198.vrp -u 20252"  # Instance: 150_200_198
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_111.vrp -u 28026"  # Instance: 180_200_111
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_120.vrp -u 26306"  # Instance: 180_200_120
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_135.vrp -u 23914"  # Instance: 180_200_135
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_137.vrp -u 25435"  # Instance: 180_200_137
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/180_200/CVRP_180_149.vrp -u 28529"  # Instance: 180_200_149
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/120_200/CVRP_120_118.vrp -u 20362"  # Instance: 120_200_118
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/120_200/CVRP_120_139.vrp -u 20034"  # Instance: 120_200_139
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/120_200/CVRP_120_40.vrp -u 20541"  # Instance: 120_200_40
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/120_200/CVRP_120_84.vrp -u 20318"  # Instance: 120_200_84
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/150_200/CVRP_150_139.vrp -u 19186"  # Instance: 150_200_139
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_83.vrp -u 27172"  # Instance: 200_200_83
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_127.vrp -u 30917"  # Instance: 200_200_127
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_56.vrp -u 23177"  # Instance: 200_200_56
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_12.vrp -u 29819"  # Instance: 200_200_12
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_194.vrp -u 24828"  # Instance: 200_200_194
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_37.vrp -u 23999"  # Instance: 200_200_37
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_168.vrp -u 27851"  # Instance: 200_200_168
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_91.vrp -u 29939"  # Instance: 200_200_91
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_144.vrp -u 30566"  # Instance: 200_200_144
    "./bin/cvrp /blue/yu.yang1/haoran.liu/instance2/200_200/CVRP_200_62.vrp -u 26334"  # Instance: 200_200_62
)


eval ${COMMANDS[$SLURM_ARRAY_TASK_ID]}