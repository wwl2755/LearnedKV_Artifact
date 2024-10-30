#!/bin/bash

# Directory containing workload spec files
WORKLOAD_DIR="workloads"

# Loop through each .spec file in the workloads directory
for spec_file in $WORKLOAD_DIR/*.spec; do
    # Get the base name of the spec file (without path and extension)
    base_name=$(basename "$spec_file" .spec)
    
    echo "Running workload: $base_name"
    
    # Run YCSB-C
    ./ycsbc -db tbb_rand -threads 4 -P "$spec_file"
    
    # Rename the output.txt file to match the workload name
    mv output.txt "../datasets/${base_name}_output.txt"
    
    echo "Completed $base_name. Output saved to ${base_name}_output.txt"
done