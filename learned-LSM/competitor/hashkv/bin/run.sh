#!/bin/sh
datasets="../../../../datasets/YCSB-A_output.txt ../../../../datasets/YCSB-B_output.txt ../../../../datasets/YCSB-C_output.txt"
for dataset in $datasets; do
    rm -f data_dir/* leveldb/*
    ./hashkv_test data_dir/ 1000000 ${dataset}
done
