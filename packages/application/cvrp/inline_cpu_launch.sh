#!/bin/bash

# Configuration
CPU_THRESHOLD=10
CHECK_INTERVAL=5  # seconds between checks
COMMAND_TO_RUN="sh launch_2B_wo_enum_bkf.sh"  # Replace with your actual command
LOG_FILE="/tmp/cpu_monitor.log"

while true; do
    cpu=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | sed 's/%us,//' | cut -d'.' -f1)
    
    if [[ $cpu -lt $CPU_THRESHOLD ]]; then
        echo "$(date) - CPU below ${CPU_THRESHOLD}% (${cpu}%), running: $COMMAND_TO_RUN"
        $COMMAND_TO_RUN
        echo "$(date) - Command completed"
        break
    else
        echo "$(date) - CPU: ${cpu}% (above threshold), waiting..."
    fi
    
    sleep $CHECK_INTERVAL
done