#!/bin/sh

# have to install rocksdb system-wide ahead
# "make install"

# use the following commands to install leveldb system-wide
# sudo apt-get update
# sudo apt-get install libleveldb-dev

# switch from rocksdb to leveldb
# g++ test.cpp -O3 -o test -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -ldl -w
g++ test.cpp -o test -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -ldl -w
# g++ test.cpp -O2 -o test -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -lleveldb -w

# various workloads
# datasets="output_1M_R7W3.txt output_1M_RW.txt output_1M_R3W7.txt output_1M_W.txt"
datasets="output_1M_RW.txt"
# datasets="output_1M_S.txt output_1M_W_S.txt output_1M_W_S_2.txt"
# datasets="output_1M_YCSB_a.txt output_1M_YCSB_b.txt output_1M_YCSB_c.txt"
# over_provision_ratios="0.1 0.2 0.3 0.4 0.5"

for dataset in $datasets; do
    rm -r db/*
    ./test --keys_file=${dataset} --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=0.3


    rm -r db/*
    ./test --keys_file=${dataset} --index_type=RocksDB --num_kv=1000000 --over_provision_ratio=0.3
done


# # default testing
# datasets="output_1M_RW.txt"
# rm -r db/*
# ./test --keys_file=${datasets} --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=0.3

# rm -r db/*
# ./test --keys_file=${datasets} --index_type=RocksDB --num_kv=1000000 --over_provision_ratio=0.3
