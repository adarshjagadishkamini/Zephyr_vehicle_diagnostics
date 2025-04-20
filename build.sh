#!/bin/bash

# Build all nodes
NODES="vcu temp_node gps_node collision_node battery_node brake_node tpms_node speed_node"

for node in $NODES; do
    echo "Building $node..."
    west build -p -b esp32s3_devkitm $node
    if [ $? -ne 0 ]; then
        echo "Error building $node"
        exit 1
    fi
    mv build/zephyr/zephyr.bin artifacts/${node}.bin
done

echo "Build complete"
