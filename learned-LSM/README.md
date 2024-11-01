# learned-LSM

unused files: group.h, learnedLSM.h, temp.cpp


```
sudo apt-get update
sudo apt-get install libsnappy-dev libbz2-dev liblz4-dev libzstd-dev
sudo apt-get install libleveldb-dev
sudo apt-get install libgmp-dev
sudo apt-get install libeigen3-dev

sudo apt-get install python3-pip
pip3 install matplotlib
pip3 install numpy pandas matplotlib seaborn

mkdir db
```

Every time the results are stored in out.csv, and some .png figures will be generated
Note: 
In order to make the figure, the out.csv will be removed before each experiment.
So previous results will not be stored.

1. Throughput Comparison between LearnedKV and RocksDB.
```
sh phase_throughput.sh
```
The result can be seen in phase_throughput.png

2. Performance of LearnedKV vs. RocksDB for Different Read: Write Ratios.
```
sh readwrite_ratio.sh
```
The result can be seen in readwrite_ratio.png

3. Multiple KV Stores Throughput Comparison for YCSB Workloads.
Before running experiments, competitors need to be set up
LearnedKV
    sh kv_store.sh
Hashkv
    cd competitor/hashkv
    Then follow the readme in the folder
Bourbon
    cd competitor/learned-leveldb
    mkdir build
    cd build
    cmake ..
    make -j
    mkdir db
    sh ../run.sh
hybrid index (still working)
    cd competitor/Efficient-LI-On-Disk
    mkdir build
    cd build
    cmake ..
    make -j
    mkdir db
    sh ../run.sh
B+-Tree
    cd competitor/B+-Tree
    mkdir db
    sh run.sh


