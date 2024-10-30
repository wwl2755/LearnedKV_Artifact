#include <iostream>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include "utils.h"
#include "../HashKV/src/kvServer.hh"

using namespace std;

#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 1016 // KV size is 1024 bytes


// NOTE: using GROUP_SIZE to represent the condition of GC
//#define GROUP_SIZE (unsigned long long)(1.3*10*1000*1000) * 1024ULL // 1.3 * 100K * 1KB = 1300MB

#define ERR_BOUND 1000

#define KEY_TYPE uint64_t
#define offset_t uint64_t

#define VLOG_PATH "db/vlog"

//size_t group_count = 0;
extern long learned_index_time;
extern long rocksdb_time;
extern long rocksdb_success_time;
extern long rocksdb_fail_time;
extern long learned_index_count;
extern long rocksdb_count;
extern long load_kv_time;
extern long load_kv_count;
extern bool learned_index_used;

extern long gc_count;
extern long gc_write_size;

extern unsigned long long group_size;

#define DISK_SIZE           (1024 * 1024 * 1024 * 1) // 1GB

class HashKV {
  public:
  DiskInfo disk1;
  std::vector<DiskInfo> disks;
  DeviceManager diskManager;
  KvServer kvserver;
  

  HashKV(){


    ConfigManager::getInstance().setConfigPath("config.ini");

    // Initialize disk1, diskManager, and kvserver using member variables
    disk1 = DiskInfo(0, "some_path", DISK_SIZE); // Replace "some_path" with the actual path

    disks.push_back(disk1);

    diskManager = DeviceManager(disks);
    kvserver = KvServer(&diskManager);
  }

  ~HashKV(){
    // Close the database
    //delete db;
    // TODO: clean up the model and key_arry files
  }



  int put(char *key, uint64_t keySize, char *value, uint64_t valueSize){

    kvserver.putValue(key,keySize, value,valueSize);


    return 0;
  }


  int get(char *key, uint64_t keySize, char *value, uint64_t valueSize){

    kvserver.getValue(key, keySize, value, valueSize);

    return 0;
  }


};
