#include "BourbonWrapper.h"
#include <cassert>
#include <iostream>
#include <random>
#include "mod/stats.h"
// #include "../../utils.h" 


#include <chrono>
#include "leveldb/db.h"
#include "leveldb/comparator.h"
#include "../mod/util.h"
#include "../mod/stats.h"
#include "../mod/learned_index.h"
#include <cstring>
#include "../mod/cxxopts.hpp"
#include <unistd.h>
#include <fstream>
#include "../db/version_set.h"
#include <cmath>
#include <random>

using namespace std;
using namespace leveldb;
using namespace adgMod;

class NumericalComparator : public Comparator {
public:
    NumericalComparator() = default;
    virtual const char* Name() const {return "adgMod:NumericalComparator";}
    virtual int Compare(const Slice& a, const Slice& b) const {
        uint64_t ia = adgMod::ExtractInteger(a.data(), a.size());
        uint64_t ib = adgMod::ExtractInteger(b.data(), b.size());
        if (ia < ib) return -1;
        else if (ia == ib) return 0;
        else return 1;
    }
    virtual void FindShortestSeparator(std::string* start, const Slice& limit) const { return; };
    virtual void FindShortSuccessor(std::string* key) const { return; };
};

BourbonWrapper::BourbonWrapper() : db(nullptr) {
    write_pos = 0;
    initialize(DB_PATH);
}

BourbonWrapper::~BourbonWrapper() {
    shutdown();
}

void BourbonWrapper::initialize(const std::string& db_location) {
    options.create_if_missing = true;
    // write_options.sync = false;

    // options.comparator = new NumericalComparator;

    // options.write_buffer_size = 64 * 1024; // 64KB
    // options.max_file_size = 16 * 1024; // 16KB 
    // options.compression = leveldb::CompressionType::kNoCompression;


    Status status = DB::Open(options, db_location, &db);
    assert(status.ok() && "Open Error");

}

std::string BourbonWrapper::get(string key, uint64_t keySize) {
    std::string value;

    // !!!! attention!! Bourbon only support string keys that can be converted to unsigned long long using std::stoull
    // string key_str = ullToString2(key);
    // string key_str = to_string(key);
    // key_str.resize(keySize);
    Status status = db->Get(read_options, key, &value);

    return value;
}

void BourbonWrapper::put(string key, uint64_t keySize, const std::string& value, uint64_t valueSize) {



    // // !!!! attention!! Bourbon only support string keys that can be converted to unsigned long long using std::stoull
    // // put into bourbon
    // // string key_str = ullToString2(key);
    // string key_str = to_string(key);
    // key_str.resize(keySize);

    // leveldb::Status status = db->Put(write_options, key_str, value);

    // std::default_random_engine e1(0), e2(255), e3(0);
    // adgMod::value_size = value.size();
    // string values(1024 * 1024, '0');
    // std::uniform_int_distribution<uint64_t > uniform_dist_value(0, (uint64_t) values.size() - adgMod::value_size - 1);

    // static uint64_t key_static = 0;
    // string key_str = to_string(key_static++);
    // string the_key = generate_key(key_str);

    // leveldb::Status status = db->Put(write_options, key_str, {values.data() + uniform_dist_value(e2), (uint64_t) adgMod::value_size});

    
    Status status = db->Put(write_options, key, value);
}

void BourbonWrapper::garbage_collection(uint64_t keySize, uint64_t valueSize) {
    // actually make it to be the learning process
    adgMod::db->vlog->Sync();

    adgMod::db->WaitForBackground();
    // delete db;
    // leveldb::Status status = leveldb::DB::Open(options, DB_PATH, &db);
    // status = leveldb::Open(options, DB_PATH, &db);
    adgMod::db->WaitForBackground();
    if (1) {
        Version* current = adgMod::db->versions_->current();

        for (int i = 1; i < config::kNumLevels; ++i) {
            adgMod::LearnedIndexData::Learn(new adgMod::VersionAndSelf{current, adgMod::db->version_count, current->learned_index_data_[i].get(), i});
        }
        current->FileLearn();
    }
    cout << "Shutting down" << endl;
    adgMod::db->WaitForBackground();
    // delete db;
    // status = leveldb::DB::Open(options, DB_PATH, &db);
    // leveldb::Options options;
    // leveldb::Status status = leveldb::RepairDB(DB_PATH, options);
}

void BourbonWrapper::shutdown() {
    if (db != nullptr) {
        adgMod::db->WaitForBackground();
        delete db;
        db = nullptr;
    }
}

void BourbonWrapper::destroy() {
    if (db != nullptr) {
        adgMod::db->WaitForBackground();
        delete db;
        db = nullptr;
    }
    int res = std::remove(DB_PATH);
}


std::string BourbonWrapper::generate_key(const std::string& key) {
    // Implement key generation logic here if needed
    return key;
}