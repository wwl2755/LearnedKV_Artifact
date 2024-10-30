#!/bin/sh

g++ rocksdb.cpp -o rocksdb -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -ldl
#./rocksdb

command="./rocksdb"

i=1
while [ $i -le 10 ]
do
    echo "Run number $i"
    $command >> output.txt  # execute the command
    i=$((i+1))  # increment the counter
    sleep 1  # Optional: wait for a second before the next run
done