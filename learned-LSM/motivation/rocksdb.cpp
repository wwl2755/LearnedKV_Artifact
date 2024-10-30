// test for motivation: 
// LSM read/write performance will be affected by the number of keys in the database

#include <chrono>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <iostream>
#include <vector>

// Define the number of keys to be inserted in each stage of the experiment
const int kNumKeysPerStage = 1000000;

// Define the number of stages in the experiment
const int kNumStages = 10;

#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 56 // 120 bytes

// Define a function to create a new key for insertion
std::string create_key(int stage, int i) {
    return "key_" + std::to_string(stage) + "_" + std::to_string(i);
}
std::string create_key_n_bytes(int stage, int i, int n) {
    std::string ret = "key_" + std::to_string(stage) + "_" + std::to_string(i);
    //padding ret to n bytes
    while(ret.size() < n) {
        ret += "0";
    }
    return ret;
}

// Define a function to create a value for insertion
std::string create_value(int stage, int i) {
    return "value_" + std::to_string(stage) + "_" + std::to_string(i);
}

std::string create_value_n_bytes(int stage, int i, int n) {
    std::string ret = "value_" + std::to_string(stage) + "_" + std::to_string(i);
    //padding ret to n bytes
    while(ret.size() < n) {
        ret += "0";
    }
    return ret;
}

int main() {
    // Create a RocksDB instance
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;

    options.level_compaction_dynamic_level_bytes = false;

    options.write_buffer_size = 256 *  1024; // memtable size = 64KB

    // disable write cache
    // rocksdb::BlockBasedTableOptions table_options;
    // table_options.block_cache = nullptr;
    // options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    // Set the maximum total data size for level-1
    options.max_bytes_for_level_base = 1024 * 1024; // 16KB

    // Set the size ratio between levels
    options.max_bytes_for_level_multiplier = 10;

    rocksdb::Status status = rocksdb::DB::Open(options, "./testdb", &db);

    if (!status.ok()) {
        std::cerr << "Error opening database: " << status.ToString() << std::endl;
        return 1;
    }

    std::vector<std::string> keys;
    std::vector<std::string> values;
    for(int stage = 0; stage < kNumStages; ++stage) {
        for(int i = 0; i < kNumKeysPerStage; ++i) {
            keys.push_back(create_key_n_bytes(stage, i, KEY_SIZE));
            values.push_back(create_value_n_bytes(stage, i, VALUE_SIZE));
        }
    }

    for (int stage = 0; stage < kNumStages; ++stage) {
        // Measure the time taken to insert kNumKeysPerStage keys
        auto start_time = std::chrono::steady_clock::now();
        for (int i = 0; i < kNumKeysPerStage; ++i) {
            rocksdb::Status s = db->Put(rocksdb::WriteOptions(), keys[stage * kNumKeysPerStage + i], values[stage * kNumKeysPerStage + i]);
            if (!s.ok()) {
                std::cerr << "Error writing key-value pair: " << s.ToString() << std::endl;
                return 1;
            }
        }
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Stage " << stage << " write time (in ms): " << elapsed_time.count() << std::endl;


        std::vector<std::string> keys_to_read;
        // collect keys to read ((stage+1) * kNumKeysPerStage) and shuffle them, 
        // then select kNumKeysPerStage keys randomly from them
        for(int i = 0; i < (stage+1) * kNumKeysPerStage; ++i) {
            keys_to_read.push_back(keys[i]);
        }
        std::random_shuffle(keys_to_read.begin(), keys_to_read.end());
        keys_to_read.resize(kNumKeysPerStage);

        // Measure the time taken to read back the keys
        start_time = std::chrono::steady_clock::now();
        for (int i = 0; i < kNumKeysPerStage; ++i) {
            std::string value;
            rocksdb::Status s = db->Get(rocksdb::ReadOptions(), keys_to_read[i], &value);
            if (!s.ok()) {
                std::cerr << "Error reading key-value pair: " << s.ToString() << std::endl;
                return 1;
            }
        }
        end_time = std::chrono::steady_clock::now();
        elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Stage " << stage << " read time (in ms): " << elapsed_time.count() << std::endl;

        // print the number of levels and the size of each level
        std::string property;
        // if (db->GetProperty("rocksdb.num-files-at-level", &property)) {
        //     std::cout << "Number of levels: " << property << std::endl;
        // }

        // wait for compaction to finish
        while(true){
            uint64_t num_running_compactions;
            db->GetIntProperty("rocksdb.num-running-compactions", &num_running_compactions);
            if(num_running_compactions == 0){
                break;
            }
        }

        
        for (int i=0;i<=6;i++){
            if (db->GetProperty("rocksdb.num-files-at-level" + std::to_string(i), &property)) {
                std::cout << "Number of files at level " << i << ": " << property << std::endl;
            }
        }

    }

    // std::string property;
    // if (db->GetProperty("rocksdb.levelstats", &property)) {
    //     std::cout << property << std::endl;
    // }

    delete db;

    status = rocksdb::DestroyDB("./testdb", rocksdb::Options());
    if (!status.ok()) {
      std::cerr << status.ToString() << std::endl;
      return 1;
    }

    return 0;
}
