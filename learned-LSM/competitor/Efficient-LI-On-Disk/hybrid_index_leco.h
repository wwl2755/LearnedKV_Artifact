// This file is used for experimental section for LearnedKV

#include <iostream>
#include <vector>
#include <random>

#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include <arpa/inet.h>

#include "./key_type.h"
#include "./ycsb_utils/benchmark.h"
#include "indexes/baseline/btree-disk.h"
#include "indexes/baseline/film.h"
#include "indexes/baseline/pgm-disk-origin.h"
#include "indexes/hybrid/dynamic/alex.h"
#include "indexes/hybrid/dynamic/btree.h"
#include "indexes/hybrid/dynamic/pgm.h"
#include "indexes/hybrid/hybrid_index.h"
#include "indexes/hybrid/static/cpr_di.h"
#include "indexes/hybrid/static/leco-page.h"
#include "indexes/hybrid/static/pgm.h"
#include "indexes/hybrid/static/rs.h"

#include "indexes/hybrid/hybrid_index.h"

#include "utils.h"
#include "direct_io.h"

using namespace std;

// typedef unsigned long long Key;
// typedef unsigned long long Value;
// typedef std::pair<Key, Value> Record;

// Define the types for DynamicPGMIndex and StaticPGMIndex
// using Dynamic = AlexIndex<Key, Value>;
using Dynamic = BTreeIndex<Key, Value>;
// using Static = StaticCprDI<Key, Value>;
using Static = StaticLecoPage<Key, Value>;


#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 1016 // KV size is 1024 bytes

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

extern long scan_kvs_count;

extern unsigned long long group_size;

class LearnedKV {
  public:

  // BaselineAlexDisk* hybrid;
  HybridIndex<Key, Value, Dynamic, Static>* hybrid_index;
  unsigned long long write_pos;

  LearnedKV(){
    initialize_rocksdb();
    write_pos=0;
  }

  ~LearnedKV(){
    // Close the database
    // delete db;
    delete hybrid_index;
    // TODO: clean up the model and key_arry files
  }
// static bool sort_func(pair<unsigned long long , unsigned long long >& a, pair<unsigned long long , unsigned long long >& b){
//     return a.first < b.first;
// }
void bulk_load (vector<pair<string, unsigned long long>> &workloads , unsigned int num_kv){
    // vector<pair<unsigned long long , unsigned long long >> key_value;
    DataVec key_value;
    
    // rewrite the vlog file
    string vlog_path = VLOG_PATH;
    // FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "ab");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;

    for (int i=0;i<num_kv;i++){


      // write to vlog
      unsigned long long key_ull = workloads[i].second;
      // int num = fwrite(&key_ull,KEY_SIZE,1,vlog);

      // char key_char = static_cast<char>(key_ull & 0xFF);
      string value;
      // Resize the string to the desired size
      value.resize(VALUE_SIZE);

      // Fill the string with the character derived from key
      // memset(&value[0], key_char, VALUE_SIZE);
      
      // num = fwrite(value.data(),VALUE_SIZE,1,vlog);

      DirectIOHelper::writeAligned(vlog, &key_ull, KEY_SIZE);
      DirectIOHelper::writeAligned(vlog, value.data(), VALUE_SIZE);  


      key_value.push_back(make_pair(workloads[i].second, write_pos));

      write_pos++;
    }
    sort(key_value.begin(), key_value.end());

    // alex->Build(key_value);
    hybrid_index->Build(key_value);


    fclose(vlog);
  }

  int initialize_rocksdb(){


    // Create the HybridIndex

    // const float kIndexParams2 = 2; // possibly 2,3,4,5
    // string kFilepath = "./db_hybrid";
    // const uint64_t kPageBytes = 4096;
    // size_t memory_budget = 10 * 1024 * 1024;

    // read_buf_ = reinterpret_cast<Key*>(aligned_alloc(kPageBytes, kPageBytes * ALLOCATED_BUF_SIZE));

    // hybrid_index = new HybridIndex<Key, Value, Dynamic, Static>({{},
    //        {kIndexParams2, kPageBytes / sizeof(Record), {kFilepath, kPageBytes}},
    //        memory_budget});

    const uint64_t kPageBytes = 4096;
    string kFilepath = "./db_hybrid";
    uint64_t fix = 0, slide = 1;

    StaticLecoPage<Key, Value>::param_t leco_para;
    leco_para = StaticLecoPage<Key, Value>::param_t{kPageBytes / sizeof(Record), fix, slide, 1000, {kFilepath, kPageBytes}};
    size_t memory_budget = 10 * 1024 * 1024; 

    read_buf_ = reinterpret_cast<Key*>(aligned_alloc(kPageBytes, kPageBytes * ALLOCATED_BUF_SIZE));
    
    hybrid_index = new HybridIndex<Key, Value, Dynamic, Static>({{}, leco_para, memory_budget});

    return 0;
  }

  int destroy_rocksdb(){
    delete hybrid_index;
    int res = std::remove("db_hybrid");
    free(read_buf_);
    return 0;
  }

  int put(unsigned long long key, uint64_t keySize, string value, uint64_t valueSize){
    // check if it can be correctly inserted

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
    // FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "ab");
    fseek(vlog, 0, SEEK_END);
    assert(write_pos*(keySize+valueSize) == (unsigned long long)ftell(vlog));
    //write_pos = (uint64_t)ftell(vlog);
    //cout<<g->write_pos<<endl;
    // fwrite(&key,keySize,1,vlog);
    // fwrite(value.data(),valueSize,1,vlog);
    DirectIOHelper::writeAligned(vlog, &key, keySize);
    DirectIOHelper::writeAligned(vlog, value.data(), valueSize);
    fclose(vlog);
    

    hybrid_index->Insert(key, write_pos);
    // hybrid_index->Insert(1001, 2001);


    assert(write_pos<100000000ULL);// write_pos should be smaller than 10^8

    //write_pos += keySize + valueSize;
    write_pos ++;


    return 0;
  }

  int get(unsigned long long key, uint64_t keySize, string& value, uint64_t valueSize){
    //char *result =(char*) malloc(1025);;
    //memset(result,0,1025);
    bool accurate = true;
    string result;
    bool rocksdb_found = false;


    // int c=0;
    // bool found = bt->lookup(key, &c);
    // bt->sync_metanode();

    unsigned long long pos = hybrid_index->Find(key);
    // Value result = hybrid_index.Find(1001);


    // string key_str = ullToString(key);
    // rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key_str, &result);

    struct timeval startTime, endTime;
    gettimeofday(&startTime, 0);




    string vlog_path = VLOG_PATH;
    // FILE * vlog = fopen (vlog_path.c_str(),"rb");
    FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}
    //cout<<g->write_pos<<endl;
    pos = pos*(keySize+valueSize);
    fseek(vlog,pos+keySize,SEEK_SET);

    value.resize(valueSize);
    // fread(&value[0], valueSize, 1, vlog);

    value.resize(valueSize);
    DirectIOHelper::readAligned(vlog, &value[0], valueSize);

    //printf("value: %s",value);
    fclose(vlog);

    gettimeofday(&endTime, 0);
    // //rocksdb_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    load_kv_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    //rocksdb_count++;
    load_kv_count++;

    return 0;
  }

  int garbage_collection(uint64_t keySize, uint64_t valueSize){

    unordered_map<unsigned long long, string> valid_map;
    string vlog_path = VLOG_PATH;
    // FILE * vlog = fopen (vlog_path.c_str(),"rb");
    FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

    fseek(vlog,0,SEEK_SET);
    char key[KEY_SIZE+1];
    // char value[VALUE_SIZE+1];
    string value;
    memset(key, 0, sizeof(key));
    // memset(value, 0, sizeof(value));
    // while (fread(key, sizeof(char), KEY_SIZE, vlog)>0) { // loop until end of file
    while (DirectIOHelper::readAligned(vlog, key, KEY_SIZE)) {
      size_t bytes_read=0;
      value.resize(VALUE_SIZE);
      // bytes_read = fread(&value[0], sizeof(char), VALUE_SIZE, vlog);
      DirectIOHelper::readAligned(vlog, &value[0], VALUE_SIZE);
      //cout<<"bytes read for value: "<<bytes_read<<endl;
      // assert(bytes_read==VALUE_SIZE);

      //string key_str = key;
      unsigned long long key_ull = 0;
      memcpy(&key_ull,key,sizeof(unsigned long long));
      string value_str = value;
      //if(valid_map.find(key_str)!=valid_map.end()){cout<<"dup!"<<key_str<<" "<<key_str.length()<<endl;}
      valid_map[key_ull] = value;
    }
    fclose(vlog);



    // erase rocksdb
    destroy_rocksdb();
    //std::cout << "Database erased successfully." << std::endl;

    int res = std::remove(vlog_path.c_str());

    // Set up LSM options
    initialize_rocksdb();
    //std::cout << "Database set up successfully." << std::endl;
    

    // rewrite the vlog file
    // vlog = fopen (vlog_path.c_str(),"a+b");
    vlog = DirectIOHelper::openFile(vlog_path.c_str(), "wb");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;
    
    vector<pair<unsigned long long, string>> valid_vec(valid_map.begin(), valid_map.end()); // key -> value

    // Sort the vector based on the keys
    sort(valid_vec.begin(), valid_vec.end());


    // vector<pair<unsigned long, unsigned long>> key_value;
    DataVec key_value;

    string ptr;
    
    for (auto it = valid_vec.begin(); it != valid_vec.end() - 1; ++it){
      //how to get the next key in the valid_vec?
      auto valid_kv = *it;


      unsigned long long key_ull = valid_kv.first;


      string value_str = valid_kv.second;

      
      // memset(key_char,0,sizeof(key_char));
      // memcpy(key_char,&key_ull,sizeof(unsigned long long));

      // // write to vlog
      // int num = fwrite(&key_ull,keySize,1,vlog);
      // // if(num!=1){cout<<"Failed to write key to vlog."<<endl;}
      // num = fwrite(valid_kv.second.data(),valueSize,1,vlog);

      DirectIOHelper::writeAligned(vlog, &key_ull, keySize);
      DirectIOHelper::writeAligned(vlog, valid_kv.second.data(), valueSize);


      // bt->Insert(key_ull, write_pos);
      key_value.push_back(make_pair(key_ull, write_pos));

      // bt->insert_key_entry(key_ull, write_pos, &sel, &sml, &inl, &smo_c, &update_c);
      // bt->sync_metanode();


      // filter.insert(key_ull);


      //write_pos += keySize + valueSize;
      write_pos++;
    }
    //key_array=x;

    // bt->Build(key_value);
    hybrid_index->Build(key_value);

    gc_write_size += (KEY_SIZE+VALUE_SIZE)*valid_vec.size();


    fclose(vlog);
    return 0;
  }
};