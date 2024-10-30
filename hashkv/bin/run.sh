#!/bin/sh
datasets="output_1M_YCSB_a.txt output_1M_YCSB_b.txt output_1M_YCSB_c.txt"
for dataset in $datasets; do
    rm -f data_dir/* leveldb/*
    ./hashkv_test data_dir/ 1000000 ${dataset}
done
