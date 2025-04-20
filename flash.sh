#!/bin/bash

NODE=$1
PORT=$2

if [ -z "$NODE" ] || [ -z "$PORT" ]; then
    echo "Usage: $0 <node_name> <port>"
    echo "Example: $0 temp_node /dev/ttyUSB0"
    exit 1
fi

echo "Flashing $NODE..."
west flash -d build_${NODE,,} --erase --port $PORT

if [ $? -eq 0 ]; then
    echo "Flash successful"
else
    echo "Flash failed"
    exit 1
fi
