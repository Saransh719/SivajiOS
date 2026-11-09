#!/usr/bin/env bash

# Move to the directory containing this script
cd "$(dirname "$0")" || exit 1

# Check if ISO exists
if [ ! -f "os.iso" ]; then
    echo "Error: os.iso not found!"
    exit 1
fi

echo "Starting OS in QEMU..."

qemu-system-i386 \
    -cdrom os.iso \
    -boot order=d \
    -m 32 \
    -display gtk \
    -monitor stdio \
    -no-reboot \
    -no-shutdown