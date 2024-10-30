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

using namespace std;

#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 1016 // KV size is 1024 bytes


// NOTE: using GROUP_SIZE to represent the condition of GC
//#define GROUP_SIZE (unsigned long long)(1.3*10*1000*1000) * 1024ULL // 1.3 * 100K * 1KB = 1300MB

#define ERR_BOUND 1000

#define KEY_TYPE uint64_t
#define offset_t uint64_t

#define VLOG_PATH "db/vlog"

#define TOTAL_SIZE 1000000

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

static int static_count = 0;


class LearnedKV {
  public:
  rocksdb::DB* db; // for key-value
  std::vector<rocksdb::ColumnFamilyHandle*> handles;

  

  LearnedKV(){
    initialize_rocksdb();
  }

  ~LearnedKV(){
    // Close the database
    delete db;
    // TODO: clean up the model and key_arry files
  }

  int initialize_rocksdb(){
    // Instantiate the ColumnFamilyOptions object
    rocksdb::ColumnFamilyOptions cf_options;

    // Enable blob files
    cf_options.enable_blob_files = true;

    // Set the minimum blob size
    cf_options.min_blob_size = 0; 

    // Set the blob file size
    cf_options.blob_file_size = 64ULL * 1024 * 1024; 

    // Set the compression type for blob files
    cf_options.blob_compression_type = rocksdb::kNoCompression;

    // Enable blob garbage collection
    cf_options.enable_blob_garbage_collection = true;

    // Set the blob garbage collection age cutoff
    cf_options.blob_garbage_collection_age_cutoff = 0.25;

    // Set the table factory for block-based table
    rocksdb::BlockBasedTableOptions table_options;
    table_options.block_cache = nullptr;
    cf_options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    // Other options
    rocksdb::DBOptions db_options;
    db_options.create_if_missing = true;
    db_options.create_missing_column_families = true;

    // Set write_buffer_size
    cf_options.write_buffer_size = 64 * 1024; // 1024/16 * 64KB

    // Set the maximum total data size for level-1
    cf_options.max_bytes_for_level_base = 64 * 1024; // 16KB

    // Set the size ratio between levels
    cf_options.max_bytes_for_level_multiplier = 10;

    // Enable direct I/O
    // db_options.use_direct_reads = true;
    // db_options.use_direct_io_for_flush_and_compaction = true;

    // Path to the database
    std::string db_path = "./db/db";

    // Create a list of column families
    std::vector<rocksdb::ColumnFamilyDescriptor> column_families;
    column_families.push_back(rocksdb::ColumnFamilyDescriptor(
        rocksdb::kDefaultColumnFamilyName,
        rocksdb::ColumnFamilyOptions()
    ));
    column_families.push_back(rocksdb::ColumnFamilyDescriptor(
        "new_cf", 
        cf_options
    ));
    
    // Open the database with the new column family
    //std::vector<rocksdb::ColumnFamilyHandle*> handles;
    //rocksdb::DB* db;
    rocksdb::Status s = rocksdb::DB::Open(db_options, db_path, column_families, &handles, &db);

    // Remember to check the status
    if (!s.ok()) {
        std::cerr << "Can't open RocksDB: " << s.ToString() << std::endl;
        return 1;
    }

    return 0;
  }

  int destroy_rocksdb(){
    // Always remember to close the handles and delete the DB when done.
    for (auto handle : handles) {
        db->DestroyColumnFamilyHandle(handle);
    }

    // Close the database.
    delete db;

    // Erase the database directory.
    // rocksdb::Options options;
    // rocksdb::Status status = rocksdb::DestroyDB("./db/db", options);
    // if (!status.ok()) {
    //     std::cerr << "Failed to erase database: " << status.ToString() << std::endl;
    //     return -1;
    // }
    return 0;
  }


  int put(unsigned long long key, uint64_t keySize, string value, uint64_t valueSize){

    string key_str = ullToString(key);
    // rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key, value);
    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), handles[1], key_str, value);

    if (!status.ok()) {
        std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
        return -1;
    }
    
    // rocksdb::WaitForCompactOptions wait_options;
    // status = db->WaitForCompact(wait_options);
    // static_count++;
    // if(static_count == TOTAL_SIZE){
    //   rocksdb::WaitForCompactOptions wait_options;
    //   status = db->WaitForCompact(wait_options);
    // }
    return 0;
  }


  int get(unsigned long long key, uint64_t keySize, string& value, uint64_t valueSize){

    //rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key, value);
    rocksdb::ReadOptions readOptions;
    // std::string valueStr;
    string key_str = ullToString(key);
    //rocksdb::Status status = db->Get(readOptions, rocksdb::Slice(key, keySize), &valueStr);
    rocksdb::Status status = db->Get(readOptions, handles[1], key_str, &value);
    

    if (status.ok()) {
        // Successfully found the value
        if (value.size() <= valueSize) {
            // Copy the found value to the provided value buffer
            // memcpy(value, valueStr.data(), valueStr.size());
            return 0;  // Successfully read the value
        } else {
            return -2;  // Value size is too large for provided buffer
        }
    } else if (status.IsNotFound()) {
        return -1;  // Key not found
    } else {
        return -3;  // Some other error
    }

    static_count++;
    if(static_count == TOTAL_SIZE){
      rocksdb::WaitForCompactOptions wait_options;
      status = db->WaitForCompact(wait_options);
    }
    return 0;
  }
  int scan(unsigned long long startKey, uint64_t keySize, vector<pair<string,string>>& kvs, uint64_t valueSize, unsigned long long scanRange){
    return 0;
  }


};
