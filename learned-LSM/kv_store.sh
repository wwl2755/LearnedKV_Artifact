g++ test_competitor.cpp -O2 -o test -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -lleveldb -w

datasets="../datasets/YCSB-A_output.txt ../datasets/YCSB-B_output.txt ../datasets/YCSB-C_output.txt"
for dataset in $datasets; do
    rm -r db/*
    ./test --keys_file=$dataset --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=0.3
done
