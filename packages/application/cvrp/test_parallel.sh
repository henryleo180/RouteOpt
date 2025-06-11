#!/bin/bash

MAX_JOBS=4  # Start with just 4 jobs for testing
echo "Testing parallel execution with $MAX_JOBS jobs"

# Simple test function
test_job() {
    local id=$1
    echo "Starting job $id"
    sleep 5  # Simulate work
    echo "Job $id completed"
}

# Run jobs in parallel
for i in {1..8}; do
    # Wait if we have too many jobs
    while [ $(jobs -r | wc -l) -ge $MAX_JOBS ]; do
        sleep 0.5
    done
    
    # Start job in background
    test_job $i &
    echo "Started background job $i"
done

echo "Waiting for all jobs..."
wait
echo "All done!"