g++ test.cpp -o test -std=c++17 -lrocksdb -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd -ldl -w
rm out.csv
D="workloada_1M_output.txt"
for dataset in $D; do
    rm -r db/*
    ./test --keys_file=../datasets/${dataset} --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=0.3


    rm -r db/*
    ./test --keys_file=../datasets/${dataset} --index_type=RocksDB --num_kv=1000000 --over_provision_ratio=0.3
done

python3 phase_throughput.py