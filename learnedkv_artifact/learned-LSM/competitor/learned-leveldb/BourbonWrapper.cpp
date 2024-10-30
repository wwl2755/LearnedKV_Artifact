#include "BourbonWrapper.h"
#include <cassert>
#include <iostream>
#include "mod/stats.h"
// #include "../../utils.h" 

using namespace std;

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

    // options.write_buffer_size = 64 * 1024; // 64KB
    // options.max_file_size = 1 * 1024; // 16KB // modified in version_set
    options.compression = leveldb::CompressionType::kNoCompression;


    leveldb::Status status = leveldb::DB::Open(options, db_location, &db);
    assert(status.ok() && "Open Error");

    // if (1) {
    //     leveldb::Version* current = adgMod::db->versions_->current();
    //     // std::cout<<"1"<<std::endl;
    //     // for (int i = 1; i < leveldb::config::kNumLevels; ++i) {
    //     //     adgMod::LearnedIndexData::Learn(new adgMod::VersionAndSelf{current, adgMod::db->version_count, current->learned_index_data_[i].get(), i});
    //     // }
    //     // current->FileLearn();
    // }
}

std::string BourbonWrapper::get(unsigned long long key, uint64_t keySize) {
    std::string value;
    uint64_t valueSize = VALUE_SIZE;
    // std::string key_str = std::to_string(key);
    // adgMod::Stats* instance = adgMod::Stats::GetInstance();

    // instance->StartTimer(4);
    // leveldb::Status status = db->Get(read_options, key_str, &value);
    // instance->PauseTimer(4);

    // if (!status.ok()) {
    //     std::cout << key << " Not Found" << std::endl;
    //     return "";
    // }

    string result;

    // !!!! attention!! Bourbon only support string keys that can be converted to unsigned long long using std::stoull
    // string key_str = ullToString2(key);
    string key_str = to_string(key);
    key_str.resize(keySize);
    leveldb::Status status = db->Get(read_options, key_str, &result);

    unsigned long long pos = 0;
    pos = stringToULL2(result);

    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}
    //cout<<g->write_pos<<endl;
    pos = pos*(keySize+valueSize);
    fseek(vlog,pos+keySize,SEEK_SET);

    value.resize(valueSize);
    fread(&value[0], valueSize, 1, vlog);

    //printf("value: %s",value);
    fclose(vlog);



    
    return value;
}

void BourbonWrapper::put(unsigned long long key, uint64_t keySize, const std::string& value, uint64_t valueSize) {
    // std::string key_str = std::to_string(key);
    // adgMod::Stats* instance = adgMod::Stats::GetInstance();

    // instance->StartTimer(10);
    // leveldb::Status status = db->Put(write_options, key_str, value);
    // instance->PauseTimer(10);

    // assert(status.ok() && "Put Error");


    if((write_pos+1)*(keySize+valueSize) > group_size){
      garbage_collection(keySize, valueSize);
      //cout<<"gc triggered in group: "<<group_id<<endl;
      //return 0;
      gc_count++;
    }
    assert((write_pos+1)*(keySize+valueSize) <= group_size);
    
    
    //insert K,V in vlog (file)
    //and insert K,P in LSM (buffer, then SST files)

    string vlog_path = VLOG_PATH;
    //std::cout<<vlog_path<<std::endl;
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog, 0, SEEK_END);
    assert(write_pos*(keySize+valueSize) == (unsigned long long)ftell(vlog));
    //write_pos = (uint64_t)ftell(vlog);
    //cout<<g->write_pos<<endl;
    fwrite(&key,keySize,1,vlog);
    fwrite(value.data(),valueSize,1,vlog);
    fclose(vlog);

    

    assert(write_pos<100000000ULL);// write_pos should be smaller than 10^8
    string ptr = ullToString2(write_pos);


    // !!!! attention!! Bourbon only support string keys that can be converted to unsigned long long using std::stoull
    // put into bourbon
    // string key_str = ullToString2(key);
    string key_str = to_string(key);
    key_str.resize(keySize);

    leveldb::Status status = db->Put(write_options, key_str, ptr);

    //write_pos += keySize + valueSize;
    write_pos ++;

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,key,sizeof(unsigned long long));
    // filter.insert(key);

    //return 0;
}

void BourbonWrapper::garbage_collection(uint64_t keySize, uint64_t valueSize) {
    unordered_map<unsigned long long, string> valid_map;
    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

    fseek(vlog,0,SEEK_SET);
    char key[KEY_SIZE+1];
    // char value[VALUE_SIZE+1];
    string value;
    memset(key, 0, sizeof(key));
    // memset(value, 0, sizeof(value));
    while (fread(key, sizeof(char), KEY_SIZE, vlog)>0) { // loop until end of file
      size_t bytes_read=0;
      value.resize(VALUE_SIZE);
      bytes_read = fread(&value[0], sizeof(char), VALUE_SIZE, vlog);
      //cout<<"bytes read for value: "<<bytes_read<<endl;
      assert(bytes_read==VALUE_SIZE);

      //string key_str = key;
      unsigned long long key_ull = 0;
      memcpy(&key_ull,key,sizeof(unsigned long long));
      string value_str = value;
      //if(valid_map.find(key_str)!=valid_map.end()){cout<<"dup!"<<key_str<<" "<<key_str.length()<<endl;}
      valid_map[key_ull] = value;
    }
    fclose(vlog);

    // erase bourbon
    destroy();
    //std::cout << "Database erased successfully." << std::endl;

    int res = std::remove(vlog_path.c_str());

    // Set up LSM options
    initialize(DB_PATH);
    //std::cout << "Database set up successfully." << std::endl;


    // rewrite the vlog file
    vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;
    
    vector<pair<unsigned long long, string>> valid_vec(valid_map.begin(), valid_map.end()); // key -> value

    // Sort the vector based on the keys
    sort(valid_vec.begin(), valid_vec.end());

    leveldb::WriteBatch batch;
    // char ptr[sizeof(unsigned long long)];
    // char key_char[KEY_SIZE+1];
    string ptr;

    for (auto it = valid_vec.begin(); it != valid_vec.end() - 1; ++it){
        //how to get the next key in the valid_vec?
        auto valid_kv = *it;


        unsigned long long key_ull = valid_kv.first;

        string value_str = valid_kv.second;


        // write to vlog
        int num = fwrite(&key_ull,keySize,1,vlog);
        // if(num!=1){cout<<"Failed to write key to vlog."<<endl;}
        num = fwrite(valid_kv.second.data(),valueSize,1,vlog);
        // if(num!=1){cout<<"Failed to write value to vlog."<<endl;}
        // if(key_ull==14643238588227784960ULL){
        //   cout<<"pos of file: "<<ftell(vlog)<<endl;
        // }


        ptr = ullToString2(write_pos);
        
        // !!!! attention!! Bourbon only support string keys that can be converted to unsigned long long using std::stoull
        // string key_str = ullToString2(key_ull);
        string key_str = to_string(key_ull);
        key_str.resize(keySize);
        // enable batch write
        batch.Put(key_str, ptr);

      
        //write_pos += keySize + valueSize;
        write_pos++;
    }
    // write_options.sync = true;
    leveldb::Status status = db->Write(write_options, &batch);
    if (!status.ok()) {
        std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
        // return -1;
    }

    // // flush the database, otherwise all the batch is in memory
    // leveldb::FlushOptions flush_options;
    // flush_options.wait = true;  // Wait for the flush to complete

    // status = db->Flush(flush_options);
    // if (!status.ok()) {
    //     std::cerr << "Failed to flush the database: " << status.ToString() << std::endl;
    // }
    // adgMod::db->vlog->Sync();
    // adgMod::db->WaitForBackground();
    // if (1) {
    //     leveldb::Version* current = adgMod::db->versions_->current();
    //     std::cout<<"Train the model"<<std::endl;
    //     for (int i = 1; i < leveldb::config::kNumLevels; ++i) {
    //         adgMod::LearnedIndexData::Learn(new adgMod::VersionAndSelf{current, adgMod::db->version_count, current->learned_index_data_[i].get(), i});
    //     }
    //     current->FileLearn();
    // }
    // adgMod::db->WaitForBackground();

    gc_write_size += (KEY_SIZE+VALUE_SIZE)*valid_vec.size();


    fclose(vlog);

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