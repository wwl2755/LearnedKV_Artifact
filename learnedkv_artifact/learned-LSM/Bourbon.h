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
#include

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


class RDB {
  public:
  rocksdb::DB* db; // for key-value

  

  RDB(){
    initialize_rocksdb();
  }

  ~RDB(){
    // Close the database
    delete db;
    // TODO: clean up the model and key_arry files
  }

  int initialize_rocksdb(){
    // rocksdb initialization

    rocksdb::Options options;
    options.create_if_missing = true;

    options.write_buffer_size = 64 *  1024; // memtable size = 64KB

    // disable write cache
    rocksdb::BlockBasedTableOptions table_options;
    table_options.block_cache = nullptr;
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    // Set the maximum total data size for level-1
    options.max_bytes_for_level_base = 16 * 1024; // 16KB

    // Set the size ratio between levels
    options.max_bytes_for_level_multiplier = 10;

    rocksdb::Status status = rocksdb::DB::Open(options, "./db/db", &db);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString() << std::endl;
      return -1;
    }


    return 0;
  }

  int destroy_rocksdb(){
    // Close the database.
    delete db;
    // Erase the database directory.
    rocksdb::Options options;
    rocksdb::Status status = rocksdb::DestroyDB("./db/db", options);
    if (!status.ok()) {
        std::cerr << "Failed to erase database: " << status.ToString() << std::endl;
        return -1;
    }
    return 0;
  }


  int put(char *key, uint64_t keySize, char *value, uint64_t valueSize){


    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);

    if (!status.ok()) {
        std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
        return -1;
    }
    

    return 0;
  }


  int get(char *key, uint64_t keySize, char *value, uint64_t valueSize){

    //rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, value);
    rocksdb::ReadOptions readOptions;
    std::string valueStr;
    rocksdb::Status status = db->Get(readOptions, rocksdb::Slice(key, keySize), &valueStr);

    if (status.ok()) {
        // Successfully found the value
        if (valueStr.size() <= valueSize) {
            // Copy the found value to the provided value buffer
            memcpy(value, valueStr.data(), valueStr.size());
            return 0;  // Successfully read the value
        } else {
            return -2;  // Value size is too large for provided buffer
        }
    } else if (status.IsNotFound()) {
        return -1;  // Key not found
    } else {
        return -3;  // Some other error
    }


    return 0;
  }


};
