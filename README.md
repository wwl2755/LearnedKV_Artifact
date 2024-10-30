# Artifact for LearnedKV

## 1. Clone the Repository

## 2. Build RocksDB
```
git clone https://github.com/facebook/rocksdb.git
cd rocksdb
git fetch --all --tags
git checkout v9.3.1
sudo make install -j
```

## 3. prepare dataset
```
mkdir datasets
cd YCSB-C

$ sudo apt-get install libtbb-dev
$ make

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib

./ycsbc -db tbb_rand -threads 4 -P workloads/workloada.spec
sh generate_datasets.sh
// all datasets will be stored in datasets directory
```

## 4. LearnedKV with RocksDB
mkdir db
cd learned-LSM
sh run.sh

## 5. Other KV stores (HashKV, Bourbon, Learned Index, B+-Tree)

