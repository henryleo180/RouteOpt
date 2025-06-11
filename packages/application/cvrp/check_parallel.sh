#!/bin/bash

echo "Checking for GNU parallel availability..."
echo "========================================="

# Check if parallel command exists
if command -v parallel >/dev/null 2>&1; then
    echo "✓ GNU parallel is installed"
    echo "Version: $(parallel --version | head -1)"
    echo "Location: $(which parallel)"
    echo ""
    
    # Test basic functionality
    echo "Testing basic functionality..."
    echo -e "1\n2\n3" | parallel echo "Job {}"
    echo ""
    
    echo "✓ GNU parallel is working correctly"
    echo "You can use the parallel version of the script"
else
    echo "✗ GNU parallel is NOT installed"
    echo ""
    echo "Alternative methods available:"
    echo "1. xargs with -P (parallel processing) - should work on most systems"
    echo "2. Background jobs with job control"
    echo ""
    
    # Check xargs parallel capability
    echo "Checking xargs parallel support..."
    if echo -e "1\n2\n3" | xargs -n 1 -P 3 -I {} sh -c 'echo "xargs job {}"' 2>/dev/null; then
        echo "✓ xargs supports parallel processing (-P flag)"
        echo "You can use the xargs fallback in the script"
    else
        echo "✗ xargs parallel processing not supported"
        echo "Will need to use background jobs method"
    fi
fi

echo ""
echo "System information:"
echo "CPU cores: $(nproc)"
echo "Current load: $(uptime | awk -F'load average:' '{print $2}')"

# Show installation commands for parallel if not available
if ! command -v parallel >/dev/null 2>&1; then
    echo ""
    echo "To install GNU parallel:"
    echo "----------------------"
    
    # Detect OS and show appropriate install command
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        case $ID in
            ubuntu|debian)
                echo "sudo apt-get update && sudo apt-get install parallel"
                ;;
            centos|rhel|fedora)
                echo "sudo yum install parallel   # (CentOS/RHEL)"
                echo "sudo dnf install parallel   # (Fedora)"
                ;;
            arch)
                echo "sudo pacman -S parallel"
                ;;
            *)
                echo "Check your distribution's package manager for 'parallel'"
                ;;
        esac
    else
        echo "Check your distribution's package manager for 'parallel'"
    fi
    
    echo ""
    echo "Or install from source:"
    echo "wget https://ftp.gnu.org/gnu/parallel/parallel-latest.tar.bz2"
    echo "tar xjf parallel-latest.tar.bz2"
    echo "cd parallel-*"
    echo "./configure && make && sudo make install"
fi