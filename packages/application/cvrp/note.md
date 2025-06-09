# Make it executable first
chmod +x cpu_monitor.sh

# Run with nohup in background
nohup ./cpu_monitor.sh > monitor.log 2>&1 &

# Check if it's running
ps aux | grep cpu_monitor


pkill -f