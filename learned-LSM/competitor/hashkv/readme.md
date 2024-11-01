# HashKV [Modified readme for reproduction]

Enabling Efficient Updates in Key-value Storage via Hashing


## Overview

The prototype is written in C++ and uses 3rd parity libraries, including
  - [Boost library](http://www.boost.org/)
  - [HdrHistogram_c](https://github.com/HdrHistogram/HdrHistogram_c)
  - [LevelDB](https://github.com/google/leveldb)
  - [Snappy](https://github.com/google/snappy)
  - [Threadpool](http://threadpool.sourceforge.net/)


### Minimal Requirements

Minimal requirement to test the prototype:
  - Ubuntu 14.04 LTS (Server)
  - 1GB RAM


## Installation

1. On Ubuntu 14.04 LTS (Server), install 
   - C++ compiler: `g++` (version 4.8.4 or above)
   - Boost library: `libboost-system-dev`, `libboost-filesystem-dev`, `libboost-thread-dev`
   - Snappy: `libsnappy-dev`
   - CMake (required to compile HdrHistogram_c): `cmake`
   - Zlib (required to compile HdrHistogram_c): `zlib1g-dev`

```Shell
$ sudo apt-get update
$ sudo apt-get install g++ libboost-system-dev libboost-filesystem-dev libboost-thread-dev libsnappy-dev cmake zlib1g-dev
```

2. Setup the environment variable for HashKV as the root directory of folder `hashkv`

```Shell
$ export HASHKV_HOME=$(pwd)
```

3. Compile HdrHistogram\_c (`libhdr_histogram.so`) under `lib/HdrHistogram_c-0.9.4`,

```Shell
$ cd ${HASHKV_HOME}/lib/HdrHistogram_c-0.9.4
$ cmake .
$ make -j
```

4. Compile LevelDB (`libleveldb.so`) under `lib/leveldb`,

```Shell
$ cd ${HASHKV_HOME}/lib/leveldb
$ make -j
```

### Testing the Prototype

1. Compile the prototype and the test program.

```Shell
$ cd ${HASHKV_HOME}
$ make -j
```
The test program is generated under `bin/` after compilation, named `hashkv_test`.

2. Before running, add path to shared libraries under `lib/leveldb/out-shared` and `lib/HdrHistogram_c-0.9.4/src`:

```Shell
$ export LD_LIBRARY_PATH="$HASHKV_HOME/lib/leveldb/out-shared:$HASHKV_HOME/lib/HdrHistogram_c-0.9.4/src:$LD_LIBRARY_PATH"
```

3. Then, switch to folder `bin/`

```Shell
$ cd ${HASHKV_HOME}/bin
```

4. Create the folder for key storage (LSM-tree), which is named `leveldb` by default

```Shell
$ mkdir leveldb
```

5. Create the folder for value storage

```Shell
$ mkdir data_dir
```

6. Clean the LSM-tree and the value storage folders before run

```Shell
$ rm -f data_dir/* leveldb/*
```


8. Run the test program of the prototype under the chosen design

```Shell
$ sh run.sh
```

You can repeat steps 6-8 to try other designs. 

*Note that since the data layout differs between designs, the LSM-tree and the value store folders must be cleared when one switches to another design.*


## Publications

See our papers for greater design details of HashKV:

  - Helen H. W. Chan, Yongkun Li, Patrick P. C. Lee, and Yinlong Xu. [HashKV: Enabling Efficient Updates in KV Storage via Hashing][atc18hashkv] (USENIX ATC 2018)


[atc18hashkv]: https://www.usenix.org/conference/atc18/presentation/chan


## MOD
In lib/threadpool/detail/pool_core.hpp, change line 353 to 
'catch(const thread_resource_error&)'

Modified Makefile and src/util/debug.cc/getTime()
