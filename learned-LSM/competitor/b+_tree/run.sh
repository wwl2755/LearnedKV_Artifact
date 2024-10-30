g++ -O3 test.cpp -std=c++17 -I./stx-btree-0.9/include -o test

datasets="output_1M_RW.txt"
./test --keys_file=../../output_1M_RW.txt --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=0.3


g++ -O3 test.cpp -std=c++17 -I./competitor/b+_tree/stx-btree-0.9/include -o test -lrocksdb
./test --keys_file=output_1M_RW.txt --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=5.3

