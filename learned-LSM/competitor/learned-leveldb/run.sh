# # clean the db before running
# rm -rf db/*

# # use the following command in the "build" dir to run the test
# ./bourbon_test --keys_file=../../../output_1M_RW.txt  --num_kv=1000000 --over_provision_ratio=0.3
# # ./bourbon_test --keys_file=../../../output_1M_YCSB_a.txt  --num_kv=1000000 --over_provision_ratio=0.3

# go to the "build" dir
# compile the test by running "cmake .." and "make"
# run this script by running "sh ../run.sh"


datasets="YCSB-A_output.txt YCSB-B_output.txt YCSB-C_output.txt"
for dataset in $datasets; do
    rm -r db/*
    ./bourbon_test --keys_file=../../../../datasets/${dataset} --index_type=LearnedKV --num_kv=1000000 --over_provision_ratio=0.3
done