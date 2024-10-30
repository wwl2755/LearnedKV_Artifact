Artifact for LearnedKV

1. git clone 

2. set up rocksdb
rocksdb 9.3.1
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
git fetch --all --tags
git checkout v9.3.1
make install

3. prepare dataset
mkdir datasets
cd YCSB
sh generate_datasets.sh

4. LearnedKV with RocksDB
mkdir db
cd learned-LSM
sh run.sh

5. Other KV stores (HashKV, Bourbon, Learned Index, B+-Tree)

