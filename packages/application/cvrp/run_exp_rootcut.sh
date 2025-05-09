#!/bin/bash
# run_experiments.sh
# Runs CVRP experiments; instances in /home/haoran/data_eda/instance2,
# logs in /home/haoran/data_eda, with root‐cut‐only logs under log_rootcut.

INSTANCE_ROOT="/home/haoran/data_eda/instance2"
BASE_LOG_DIR="/home/haoran/data_eda/robustc"
ROOTCUT_LOG_DIR="$BASE_LOG_DIR/log_rootcut"

# create log directories if they don't exist
mkdir -p "$BASE_LOG_DIR" "$ROOTCUT_LOG_DIR"

# count total runs (3 per file)
total_runs=0
for sub in 180_200 200_200; do
  for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
    total_runs=$((total_runs + 2))
  done
done

echo "Starting experiments: Total runs = $total_runs"

show_progress() {
  local current=$1
  local percent=$(( current * 100 / total_runs ))
  local bar_length=30
  local filled=$(( bar_length * current / total_runs ))
  local bar=$(printf '%0.s#' $(seq 1 $filled))
  local empty=$(printf '%0.s ' $(seq 1 $((bar_length - filled))))
  echo -ne "\rProgress: [${bar}${empty}] ${percent}% (${current}/${total_runs})"
}

run_count=0

for sub in 180_200 200_200; do
  for file in "$INSTANCE_ROOT/$sub"/*.vrp; do
    # extract opt value
    opt=$(grep -Eo "opt *= *[0-9]+(\.[0-9]+)?" "$file" \
          | grep -Eo "[0-9]+(\.[0-9]+)?")
    if [ -z "$opt" ]; then
      echo "Warning: no opt found in $file; skipping."
      continue
    fi

    # compute u = opt + 1
    u=$(awk -v o="$opt" 'BEGIN { printf "%.0f", o + 1 }')

    base=$(basename "$file" .vrp)

    # 1) standard variant
    run_count=$((run_count+1))
    show_progress "$run_count"
    ./asaved_exe/cvrp "$file" -u "$u" \
      > "$BASE_LOG_DIR/output_${base}.txt" 2>&1

    # 2) root-cut-only variant
    # run_count=$((run_count+1))
    # show_progress "$run_count"
    # ./asaved_exe/cvrp_rootcutonly "$file" -u "$u" \
    #   > "$ROOTCUT_LOG_DIR/output_${base}_onlyr.txt" 2>&1

    # 3) robust-child variant
    run_count=$((run_count+1))
    show_progress "$run_count"
    ./asaved_exe/cvrp_robustchild "$file" -u "$u" \
      > "$BASE_LOG_DIR/output_${base}_robustc.txt" 2>&1

  done
done

# final update
show_progress "$total_runs"
echo -e "\nAll runs completed. Logs in $BASE_LOG_DIR and $ROOTCUT_LOG_DIR"

