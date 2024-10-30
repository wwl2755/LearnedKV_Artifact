#!/bin/sh

# # scan test
# datasets="../output_100K_RW.txt"

# rm -r db/*
# rm index_name
# rm btree_disk
# rm alex_disk
# rm alex_disk_data
# # ./test --keys_file=../output_100K_RW.txt  --num_kv=100000 --over_provision_ratio=0.3


datasets="../../learned-LSM/output_1M_YCSB_a.txt ../../learned-LSM/output_1M_YCSB_b.txt ../../learned-LSM/output_1M_YCSB_c.txt"
for dataset in $datasets; do
    rm -r db/*
    rm db_hybrid
    ./test --keys_file=$dataset --num_kv=1000000 --over_provision_ratio=0.3
done