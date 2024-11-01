#!/bin/sh

# # scan test
# datasets="../output_100K_RW.txt"

# rm -r db/*
# rm index_name
# rm btree_disk
# rm alex_disk
# rm alex_disk_data
# # ./test --keys_file=../output_100K_RW.txt  --num_kv=100000 --over_provision_ratio=0.3


datasets="../../../../datasets/YCSB-A_output.txt ../../../../datasets/YCSB-B_output.txt ../../../../datasets/YCSB-C_output.txt"
for dataset in $datasets; do
    rm -r db/*
    rm db_hybrid
    ./test_PGM --keys_file=$dataset --num_kv=1000000 --over_provision_ratio=0.3
done

for dataset in $datasets; do
    rm -r db/*
    rm db_hybrid
    ./test_Leco --keys_file=$dataset --num_kv=1000000 --over_provision_ratio=0.3
done