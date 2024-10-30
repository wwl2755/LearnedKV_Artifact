#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <algorithm>
#include <fstream>

const int KEY_SIZE = 16;
const int VALUE_SIZE = 16;
// const int NUM_INITIAL_KEYS[] = {2000000, 4000000, 6000000, 8000000, 1000000};
const int NUM_INITIAL_KEYS[] = {1000000, 5000000, 10000000};
const int NUM_OPERATIONS = 1000000;

std::string create_key(int i) {
    return "key_" + std::to_string(i) + std::string(KEY_SIZE - 4 - std::to_string(i).length(), '0');
}

std::string create_value(int i) {
    return "value_" + std::to_string(i) + std::string(VALUE_SIZE - 6 - std::to_string(i).length(), '0');
}

void run_test(int initial_keys, std::ofstream& csv_file) {
    rocksdb::DB* db;
    rocksdb::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 64 *  1024; // memtable size = 64KB
    options.target_file_size_base = 64 * 1024; // 16KB
    options.max_bytes_for_level_base = 256 * 1024; // 16KB
    options.level_compaction_dynamic_level_bytes = false;
    // // Enable direct I/O
    // options.use_direct_reads = true;
    // options.use_direct_io_for_flush_and_compaction = true;

    std::string db_path = "./testdb_" + std::to_string(initial_keys);
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        std::cerr << "Error opening database: " << status.ToString() << std::endl;
        return;
    }

    // Insert initial keys
    for (int i = 0; i < initial_keys; ++i) {
        db->Put(rocksdb::WriteOptions(), create_key(i), create_value(i));
    }

    // Wait for any initial compactions to finish
    db->WaitForCompact(rocksdb::WaitForCompactOptions());

    // Prepare keys for read operations
    std::vector<std::string> read_keys;
    for (int i = 0; i < initial_keys; ++i) {
        read_keys.push_back(create_key(i));
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(read_keys.begin(), read_keys.end(), g);
    read_keys.resize(NUM_OPERATIONS);

    // Read operations
    auto start_time = std::chrono::steady_clock::now();
    for (const auto& key : read_keys) {
        std::string value;
        db->Get(rocksdb::ReadOptions(), key, &value);
    }
    auto end_time = std::chrono::steady_clock::now();
    auto read_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double read_throughput = NUM_OPERATIONS * 1000.0 / read_time.count();

    std::cout << "Instance with " << initial_keys << " initial keys:" << std::endl;
    std::cout << "  Read time: " << read_time.count() << " ms" << std::endl;
    std::cout << "  Read throughput: " << read_throughput << " ops/s" << std::endl;

    // Write operations
    start_time = std::chrono::steady_clock::now();
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        db->Put(rocksdb::WriteOptions(), create_key(initial_keys + i), create_value(initial_keys + i));
    }
    
    // Wait for any background compactions to finish
    db->WaitForCompact(rocksdb::WaitForCompactOptions());
    
    end_time = std::chrono::steady_clock::now();
    auto write_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double write_throughput = NUM_OPERATIONS * 1000.0 / write_time.count();

    std::cout << "  Write time (including compactions): " << write_time.count() << " ms" << std::endl;
    std::cout << "  Write throughput: " << write_throughput << " ops/s" << std::endl;

    // Write results to CSV file
    csv_file << initial_keys << "," 
             << read_time.count() << "," << read_throughput << "," 
             << write_time.count() << "," << write_throughput << std::endl;

    delete db;
    rocksdb::DestroyDB(db_path, options);
}

int main() {
    std::ofstream csv_file("lsm_performance_results.csv");
    csv_file << "Initial Keys,Read Time (ms),Read Throughput (ops/s),Write Time (ms),Write Throughput (ops/s)" << std::endl;

    for (int initial_keys : NUM_INITIAL_KEYS) {
        run_test(initial_keys, csv_file);
        std::cout << std::endl;
    }

    csv_file.close();
    return 0;
}