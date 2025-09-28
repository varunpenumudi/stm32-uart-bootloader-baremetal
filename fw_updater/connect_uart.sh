#!/bin/bash
# Script to load cp210x USB-to-Serial driver

echo "Loading cp210x kernel module..."
sudo modprobe cp210x
dmesg | grep cp210x | tail -n 4

# Check if it loaded
if lsmod | grep -q cp210x; then
    echo "✅ cp210x module loaded successfully."
else
    echo "❌ Failed to load cp210x module."
fi
