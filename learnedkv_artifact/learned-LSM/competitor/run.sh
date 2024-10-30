#!/bin/sh

# switch from rocksdb to leveldb
# g++ test.cpp -g -o test -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd

g++ -O3 test.cpp -std=c++17 -o test -g

# # scan test
# datasets="../output_100K_RW.txt"

# rm -r db/*
# rm index_name
# rm btree_disk
# rm alex_disk
# rm alex_disk_data
# # ./test --keys_file=../output_100K_RW.txt  --num_kv=100000 --over_provision_ratio=0.3
# # ./test --keys_file=../output_1M_RW.txt  --num_kv=1000000 --over_provision_ratio=0.3

datasets="../output_1M_YCSB_a.txt ../output_1M_YCSB_b.txt ../output_1M_YCSB_c.txt"
for dataset in $datasets; do
    rm -r db/*
    ./test --keys_file=$dataset --num_kv=1000000 --over_provision_ratio=0.3
done
