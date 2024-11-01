#!/bin/sh

g++ -O3 test.cpp -std=c++17 -o test -w


datasets="../../../datasets/YCSB-A_output.txt ../../../datasets/YCSB-B_output.txt ../../../datasets/YCSB-C_output.txt"
for dataset in $datasets; do
    rm -r db/*
    ./test --keys_file=$dataset --num_kv=1000000 --over_provision_ratio=0.3
done
