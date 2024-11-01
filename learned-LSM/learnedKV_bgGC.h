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
#include <arpa/inet.h>
#include "rocksdb/statistics.h"
// #include "direct_io.h"
#include <thread>
#include <atomic>
#include <mutex>

// only tested for YCSB workloads

using namespace std;

#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 1016 // KV size is 1024 bytes


// NOTE: using GROUP_SIZE to represent the condition of GC
//#define GROUP_SIZE (unsigned long long)(1.3*10*1000*1000) * 1024ULL // 1.3 * 100K * 1KB = 1300MB

#define ERR_BOUND 1000

#define KEY_TYPE uint64_t
#define offset_t uint64_t

#define VLOG_PATH "db/vlog"
#define LI_VLOG_PATH "db/li_vlog"
#define NEW_VLOG_PATH "db/new_vlog"
#define NEW_LI_VLOG_PATH "db/new_li_vlog"

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

extern uint64_t bytes_written;
extern uint64_t bytes_written_compaction;
extern uint64_t bytes_written_learned_index;

struct Segment {
    KEY_TYPE start;
    double slope;
    double intercept;
};

class KeyPtr{
  KEY_TYPE key;
  uint64_t* ptr;
};

class LearnedKV {
  public:
  rocksdb::DB* db; // for key-ptr index
  //char* vlog; // points to the vlog file; can be cast to FILE*

  //KEY_TYPE pivot;

  // PLR models m;
  // vector<Segment> model;
  string model_str; // the path of the model file
  string new_model_str; // temporary model file during GC

  // record insert position of vlog // presented by the num of kv that have been already inserted
  unsigned long long total_num; // sum of both vlog

  //KeyPtr* key_arry; //Can be optimized later
  //vector<unsigned long long> key_array;
  string key_array_str; // the path of the key_array file
  string new_key_array_str; // the path of the key_array file

  BloomFilter filter{100000000, 7}; // 100,000,000 bits, 7 hash functions

  rocksdb::DB* new_db; // Secondary RocksDB instance during GC
  std::atomic<bool> gc_in_progress;
  std::atomic<bool> gc_done;
  std::mutex db_mutex;
  std::thread gc_thread;

  // only available during GC
  vector<Segment> current_model;
  std::mutex model_mutex;

  LearnedKV(){
    initialize_rocksdb();
    // write_pos=0;
    total_num = 0;
    model_str = "./db/model";
    new_model_str = "./db/new_model";
    key_array_str = "./db/key_array";
    new_key_array_str = "./db/new_key_array";
    gc_in_progress = false;
    gc_done = false;
  }

  ~LearnedKV(){
    // Close the database
    delete db;
    if(new_db){
      delete new_db;
    }
    // TODO: clean up the model and key_arry files
  }

  int initialize_rocksdb(){
    // rocksdb initialization

    rocksdb::Options options;
    options.create_if_missing = true;

    // Enable direct I/O
    options.use_direct_reads = true;
    options.use_direct_io_for_flush_and_compaction = true;

    options.write_buffer_size = 64 *  1024; // memtable size = 64KB
    // options.write_buffer_size = 2 * 1024 *  1024; // memtable size = 64KB

    // disable write cache
    rocksdb::BlockBasedTableOptions table_options;
    table_options.block_cache = nullptr;
    // table_options.block_cache = rocksdb::NewLRUCache(8 * 1024 * 1024LL);
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    // Set the maximum total data size for level-1
    options.max_bytes_for_level_base = 16 * 1024; // 16KB

    // Set the size ratio between levels
    options.max_bytes_for_level_multiplier = 10;

    // disable compression
    options.compression = rocksdb::kNoCompression;

    // diable level compaction
    options.level_compaction_dynamic_level_bytes = false;

    // // only for collecting statistics
    // options.statistics = rocksdb::CreateDBStatistics();


    rocksdb::Status status = rocksdb::DB::Open(options, "./db/db", &db);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString() << std::endl;
      return -1;
    }

    filter.reset();

    return 0;
  }

  int destroy_rocksdb(){

    // // only for collecting statistics
    // std::shared_ptr<rocksdb::Statistics> statistics = db->GetDBOptions().statistics;
    // bytes_written_compaction += statistics->getTickerCount(rocksdb::COMPACT_WRITE_BYTES);

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

  // x: list of key; y: list of key's offsets
  // assuming x are sorted in ascending order and x.size()>=2
  // offset (y) should be casted to double before usage
  int build_piecewise_linear_regression(const std::vector<unsigned long long>& x, const std::vector<double>& y, double err) {
    if(x.size()<2){cout<<"Size of dataset is smaller than 2."<<endl; return -1;}
    // model.clear();
    vector<Segment> model;
    uint64_t last_start = x[0];
    double a = static_cast<double>(y[1] - y[0]) / (x[1] - x[0]);
    double b = static_cast<double>(y[0] * x[1] - x[0] * y[1]) / (x[1] - x[0]);
    Segment prev_seg = { x[0], a, b };
    for (size_t i = 2; i < x.size()-1; i++) {
        if(abs(prev_seg.slope * x[i] + prev_seg.intercept - y[i]) > err){
          model.push_back({ last_start, prev_seg.slope, prev_seg.intercept });
          last_start = x[i];
          double ak = static_cast<double>(y[i+1] - y[i]) / (x[i+1] - x[i]);
          double bk = static_cast<double>(y[i] * x[i+1] - x[i] * y[i+1]) / (x[i+1] - x[i]);
          prev_seg = { x[i], ak, bk };
        }
    }
    model.push_back({ last_start, prev_seg.slope, prev_seg.intercept });

    // corner case
    if(abs(prev_seg.slope * x[x.size()-1] + prev_seg.intercept - y[x.size()-1]) > err){
        double ak = static_cast<double>(y[x.size()-1] - y[x.size()-2]) / (x[x.size()-1] - x[x.size()-2]);
        double bk = static_cast<double>(y[x.size()-2] * x[x.size()-1] - x[x.size()-2] * y[x.size()-1]) / (x[x.size()-1] - x[x.size()-2]);
        last_start = x[x.size()-2];
        prev_seg = { x[x.size()-2], ak, bk };
        model.push_back({ last_start, prev_seg.slope, prev_seg.intercept });
    }

    // write model to file in 
    FILE * model_file = fopen(new_model_str.c_str(),"wb");
    if(model_file==NULL){
      cout<<"model file does not exist."<<endl;
      return -1;
    }
    fwrite(model.data(),model.size()*sizeof(Segment),1,model_file);
    // for(size_t i = 0;i<model.size();i++){
    //   fwrite(&model[i],sizeof(Segment),1,model_file);
    // }
    fclose(model_file);

    // write key array to file
    FILE * key_array_file = fopen(new_key_array_str.c_str(),"wb");
    if(key_array_file==NULL){
      cout<<"key array file does not exist."<<endl;
      return -1;
    }
    fwrite(x.data(),x.size()*sizeof(unsigned long long),1,key_array_file);
    // for(size_t i = 0;i<x.size();i++){
    //   fwrite(&x[i],sizeof(unsigned long long),1,key_array_file);
    // }

    // FILE* key_array_file = DirectIOHelper::openFile(key_array_str.c_str(), "wb", true);
    // DirectIOHelper::writeAligned(key_array_file, x.data(), x.size() * sizeof(unsigned long long));
    fclose(key_array_file);

    return 0;
  }

  // TODO: now it is a linear search, need to be optimized; maybe binary search
  int query_model(unsigned long long key, string model_str){
    // open model file in read mode
    FILE * model_file = fopen(model_str.c_str(),"rb");
    if(model_file==NULL){
      cout<<"model file does not exist."<<endl;
      return -1;
    }
    // read model file
    //model.clear();
    vector<Segment> model;
    Segment seg;
    
    while(fread(&seg,sizeof(Segment),1,model_file)){
      model.push_back(seg);
    }
    fclose(model_file);

    // locate the correct model
    // TODO: change to binary search
    int pos = 0;
    double slope=0;
    double intercept=0;
    for(size_t i = 0;i<model.size();i++){
      if(key<model[i].start){
        break;
      }
      else if(key==model[i].start){
        slope = model[i].slope;
        intercept = model[i].intercept;
        break;
      }
      else{ // key>model[i].start
        slope = model[i].slope;
        intercept = model[i].intercept;
      }
    }
    pos = slope * key + intercept;
    // // Binary search
    // int left = 0;
    // int right = model.size() - 1;
    // while (left <= right) {
    //     int mid = left + (right - left) / 2;
    //     if (key < model[mid].start) {
    //         right = mid - 1;
    //     } else {
    //         left = mid + 1;
    //     }
    // }
    // // pos = slope * key + intercept;
    // pos = model[right].slope * key + model[right].intercept;
    if (pos<0){return 0;}
    return pos;
  }

  void mergeKeyValueVectors(const std::vector<std::pair<std::string, std::string>>& kvs_lsm,
                          const std::vector<std::pair<std::string, std::string>>& kvs_learned,
                          std::vector<std::pair<std::string, std::string>>& kvs) {
    auto lsm_it = kvs_lsm.begin();
    auto learned_it = kvs_learned.begin();

    while (lsm_it != kvs_lsm.end() && learned_it != kvs_learned.end()) {
        unsigned long long lsm_key = stringToULL(lsm_it->first);
        unsigned long long learned_key = stringToULL(learned_it->first);
        // cout<<"lsm key: "<<lsm_key<<endl;
        // cout<<"learned key: "<<learned_key<<endl;

        if (lsm_key < learned_key) {
            kvs.push_back(*lsm_it);
            ++lsm_it;
            // cout<<"lsm key is smaller."<<endl;
        } else if (lsm_key > learned_key) {
            kvs.push_back(*learned_it);
            ++learned_it;
            // cout<<"learned key is smaller."<<endl;
        } else {
            // If keys are equal, prefer the one from kvs_lsm
            kvs.push_back(*lsm_it);
            ++lsm_it;
            ++learned_it;
            // cout<<"keys are equal."<<endl;
        }
    }

    // Append remaining elements
    kvs.insert(kvs.end(), lsm_it, kvs_lsm.end());
    kvs.insert(kvs.end(), learned_it, kvs_learned.end());
  }

  int put(unsigned long long key, uint64_t keySize, string value, uint64_t valueSize){
    if(gc_done){
      detroy_old_instance();
      gc_done = false;
    }
    // check if it can be correctly inserted
    if((total_num+1)*(keySize+valueSize) > group_size && !gc_in_progress){
      // garbage_collection(keySize, valueSize);
      // gc_count++;

      gc_in_progress = true;
      initialize_new_rocksdb();
      // write_pos = 0;
      // create_new_vlog();
      
      // Start background GC thread
      gc_thread = std::thread(&LearnedKV::garbage_collection, this, keySize, valueSize);
      gc_count++;
    }
    
    
    //insert K,V in vlog (file)
    //and insert K,P in LSM (buffer, then SST files)
    string vlog_path;
    if(gc_in_progress){
      vlog_path = NEW_VLOG_PATH;
    }
    else{
      vlog_path = VLOG_PATH;
    }
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog, 0, SEEK_END);

    size_t file_size = ftell(vlog);
    unsigned long long write_pos = file_size / (keySize + valueSize);

    fwrite(&key,keySize,1,vlog);
    fwrite(value.data(),valueSize,1,vlog);
    // DirectIOHelper::writeAligned(vlog, &key, keySize);
    // DirectIOHelper::writeAligned(vlog, value.data(), valueSize);
    fclose(vlog);
    
    // assert(write_pos<100000000ULL);// write_pos should be smaller than 10^8
    string ptr = ullToString(write_pos);

    string key_str = ullToString(key);
    rocksdb::Status status;
    if(gc_in_progress) {
        status = new_db->Put(rocksdb::WriteOptions(), key_str, ptr);
    } else {
        status = db->Put(rocksdb::WriteOptions(), key_str, ptr);
    }
    // rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key_str, ptr);
    if (!status.ok()) {
        std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
        return -1;
    }
    
    //write_pos += keySize + valueSize;
    total_num ++;

    return 0;
  }


  int get(unsigned long long key, uint64_t keySize, string& value, uint64_t valueSize){
    if(gc_done){
      detroy_old_instance();
      gc_done = false;
    }
    bool accurate = true;
    string result;
    bool rocksdb_found = false;

    if (true){
      string key_str = ullToString(key);
      rocksdb::Status status;
      if(gc_in_progress && new_db != nullptr){
        status = new_db->Get(rocksdb::ReadOptions(), key_str, &result);
      }

      if (!status.ok()) {
          status = db->Get(rocksdb::ReadOptions(), key_str, &result);
      }
      rocksdb_count++;
      if (status.ok()){
        rocksdb_found = true;
      }
    }
    
    if (!rocksdb_found) {
      //std::cerr << "Failed to read data from database: " << status.ToString() << std::endl;
      
      if(learned_index_used){


        //look up in linear models
        
        accurate = false;
        long long int pos = query_model(key, model_str);
        size_t kv_size = KEY_SIZE + VALUE_SIZE;
        long long int start_pos = max((pos - ERR_BOUND),0LL)*(kv_size);
        //start_pos = ceil((double)start_pos/kv_size) * kv_size; // round to multiple of kv_size
        long long int end_pos = (pos + ERR_BOUND)*(kv_size);

        // load key array chunk from key_array_path
        FILE * key_array_file = fopen(key_array_str.c_str(),"rb");
        fseek(key_array_file, 0, SEEK_END);
        size_t key_array_size = ftell(key_array_file);
        fseek(key_array_file, KEY_SIZE*(start_pos/kv_size), SEEK_SET);
        size_t key_array_chunk_size = KEY_SIZE*(end_pos/kv_size - start_pos/kv_size); // in bytes
        unsigned long long * key_array = new unsigned long long [key_array_chunk_size/sizeof(unsigned long long)];

        fread(key_array, key_array_chunk_size, 1, key_array_file);
        fclose(key_array_file);
        
        size_t idx = 0;

        for (; idx < key_array_chunk_size/sizeof(unsigned long long); ++idx){
          if(key_array[idx]==key){
            //cout<<"found key in key array."<<endl;
            //accurate = true;
            break;
          }
        }
        start_pos = start_pos + idx * kv_size;

        delete[] key_array;
        


        learned_index_count++;

        // struct timeval load_startTime, load_endTime;
        // gettimeofday(&load_startTime, 0);
        string vlog_path;

        vlog_path = LI_VLOG_PATH;
        FILE * vlog = fopen (vlog_path.c_str(),"r+b");
        // FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "rb");
        if(vlog==NULL){cout<<"Cannot open the file"<<endl;}


        char read_key[KEY_SIZE+1];
        fseek(vlog,start_pos,SEEK_SET);
        fread(read_key, keySize, 1, vlog);
        unsigned long long read_key_ull = 0;
        memcpy(&read_key_ull,read_key,sizeof(unsigned long long));
        // if(key==14643238588227784960ULL){
        //   cout<<"read key: "<<read_key_ull<<endl;
        // }
        
        value.resize(valueSize);
        fread(&value[0], valueSize, 1, vlog);
        fclose(vlog);

        // gettimeofday(&load_endTime, 0);

        // load_kv_time += (load_endTime.tv_sec - load_startTime.tv_sec) * 1000000 + (load_endTime.tv_usec - load_startTime.tv_usec);
        load_kv_count++;

        

        return 0;
      }
      else{
        // std::cerr << "Failed to read data from database: " << std::endl;
        // unsigned long long key_ull = 0;
        // memcpy(&key_ull,key,sizeof(unsigned long long));
        // cout<<"Missed key: "<<key<<endl;
        return -1;
      }

    }
    
    // struct timeval startTime_load, endTime_load;
    // gettimeofday(&startTime_load, 0);

    unsigned long long pos = 0;



    pos = stringToULL(result);


    string vlog_path;
    if(gc_in_progress){
      vlog_path = NEW_VLOG_PATH;
    }
    else{
      vlog_path = VLOG_PATH;
    }
    FILE * vlog = fopen (vlog_path.c_str(),"rb");
    // FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}
    //cout<<g->write_pos<<endl;
    pos = pos*(keySize+valueSize);
    fseek(vlog,pos+keySize,SEEK_SET);

    value.resize(valueSize);
    fread(&value[0], valueSize, 1, vlog);

    // value.resize(valueSize);
    // DirectIOHelper::readAligned(vlog, &value[0], valueSize);
    //printf("value: %s",value);
    fclose(vlog);

    // gettimeofday(&endTime_load, 0);
    // // //rocksdb_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // load_kv_time += (endTime_load.tv_sec - startTime_load.tv_sec) * 1000000 + (endTime_load.tv_usec - startTime_load.tv_usec);
    // // rocksdb_count++;
    load_kv_count++;

    return 0;
  }

  int garbage_collection(uint64_t keySize, uint64_t valueSize){
    

    // TODO: collect valid data and re-build LSM
    unordered_map<unsigned long long, string> valid_map;
    string vlog_path = LI_VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"rb");
    if(vlog!=NULL){
      fseek(vlog,0,SEEK_SET);
      char key[KEY_SIZE+1];
      string value;
      memset(key, 0, sizeof(key));
      while (fread(key, sizeof(char), KEY_SIZE, vlog)>0) { // loop until end of file
        size_t bytes_read=0;
        value.resize(VALUE_SIZE);
        bytes_read = fread(&value[0], sizeof(char), VALUE_SIZE, vlog);
        assert(bytes_read==VALUE_SIZE);
        unsigned long long key_ull = 0;
        memcpy(&key_ull,key,sizeof(unsigned long long));
        string value_str = value;
        valid_map[key_ull] = value;
      }
      fclose(vlog);
    }

    vlog_path = VLOG_PATH;
    vlog = fopen (vlog_path.c_str(),"rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}
    fseek(vlog,0,SEEK_SET);
    char key[KEY_SIZE+1];
    string value;
    memset(key, 0, sizeof(key));
    while (fread(key, sizeof(char), KEY_SIZE, vlog)>0) { // loop until end of file
      size_t bytes_read=0;
      value.resize(VALUE_SIZE);
      bytes_read = fread(&value[0], sizeof(char), VALUE_SIZE, vlog);
      assert(bytes_read==VALUE_SIZE);
      unsigned long long key_ull = 0;
      memcpy(&key_ull,key,sizeof(unsigned long long));
      string value_str = value;
      valid_map[key_ull] = value;
    }
    fclose(vlog);


    // erase rocksdb
    // destroy_rocksdb();
    //std::cout << "Database erased successfully." << std::endl;

    // int res = std::remove(vlog_path.c_str());

    // Set up LSM options
    // initialize_rocksdb();
    //std::cout << "Database set up successfully." << std::endl;
    

    // rewrite the vlog file
    vlog_path = NEW_LI_VLOG_PATH;
    vlog = fopen (vlog_path.c_str(),"a+b");
    // vlog = DirectIOHelper::openFile(vlog_path.c_str(), "wb");
    fseek(vlog,0,SEEK_SET);
    unsigned long long write_pos = 0;
    
    vector<pair<unsigned long long, string>> valid_vec(valid_map.begin(), valid_map.end()); // key -> value

    // Sort the vector based on the keys
    sort(valid_vec.begin(), valid_vec.end());

    vector<unsigned long long> x; x.clear();
    vector<double> y; y.clear();

    string ptr;
    
    for (auto it = valid_vec.begin(); it != valid_vec.end() - 1; ++it){
      //how to get the next key in the valid_vec?
      auto valid_kv = *it;


      unsigned long long key_ull = valid_kv.first;

      string value_str = valid_kv.second;

      
      // memset(key_char,0,sizeof(key_char));
      // memcpy(key_char,&key_ull,sizeof(unsigned long long));

      // write to vlog
      int num = fwrite(&key_ull,keySize,1,vlog);
      // if(num!=1){cout<<"Failed to write key to vlog."<<endl;}
      num = fwrite(valid_kv.second.data(),valueSize,1,vlog);


      if(learned_index_used){
        x.push_back(key_ull);
        //cout<<"ull: "<<ull<<endl;
        y.push_back((double)write_pos);
      }

      
      //write_pos += keySize + valueSize;
      total_num++;
    }
    //key_array=x;


    // TODO: build learned index
    if(learned_index_used){
      build_piecewise_linear_regression(x, y, ERR_BOUND);

      // // only for collecting statistics
      // bytes_written_learned_index += (8)*valid_vec.size();
    }

    gc_write_size += (KEY_SIZE+VALUE_SIZE)*valid_vec.size();


    fclose(vlog);
    gc_done = true;
    return 0;
  }


  // keys,values,valueSizes should be empty before calling this function
  int scan(unsigned long long startKey, uint64_t keySize, vector<pair<string,string>>& kvs, uint64_t valueSize, unsigned long long scanRange){

    
    // part 1: scan the range from both LSM and Learned Index
    // part 2: merge the results

    vector<pair<string,string>> kvs_lsm;

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,startKey,sizeof(unsigned long long));
    // cout<<"start key: "<<startKey<<endl;

    
    // unsigned long long pos = 0;
    // // char result_char[sizeof(unsigned long long)+1];
    // // memset(result_char,0,sizeof(result_char));
    // memcpy(&pos, result.data(), sizeof(unsigned long long));
    // // strncpy(result_char,result.c_str(),sizeof(unsigned long long));
    // // pos = stoull(result_char);
    // pos = pos*(keySize+valueSize);

    // cout<<"pos: "<<pos<<endl;

    // unsigned long long end_key_ull = key_ull + scanRange;
    unsigned long long end_key_ull;
    if (startKey > ULLONG_MAX - scanRange) {
        end_key_ull = ULLONG_MAX; // Set to maximum value if overflow would occur
    } else {
        end_key_ull = startKey + scanRange;
    }

    //convert end_key_ull to string
    // we use stringToULL and ullToString to convert between string and unsigned long long
    // because ull will place LSB as the beginning byte
    // so the comparison 

    std::string start_key_str(sizeof(unsigned long long), '\0');
    start_key_str = ullToString(startKey);
    // memcpy(&start_key_str[0], &startKey, sizeof(unsigned long long));

    std::string end_key_str(sizeof(unsigned long long), '\0');
    end_key_str = ullToString(end_key_ull);
    // memcpy(&end_key_str[0], &end_key_ull, sizeof(unsigned long long));

    // char end_key[KEY_SIZE+1];
    // memset(end_key,0,sizeof(end_key));
    // memcpy(end_key,&end_key_ull,sizeof(unsigned long long));
    // std::string endKeyStr(end_key);

    // cout<<"end key: "<<end_key_ull<<endl;


    // part 1a: scan the range from LSM
    // Create an iterator
    rocksdb::Iterator* it = db->NewIterator(rocksdb::ReadOptions());

    // Seek to the start of the range
    for (it->Seek(start_key_str); it->Valid(); it->Next()) {
      auto itStr = it->key().ToString();
      // if (itStr > end_key_str) {
      //     break;
      // }

      unsigned long long it_ull = 0;
      it_ull = stringToULL(itStr);
      // // memcpy(&it_ull,itStr.data(),sizeof(unsigned long long));
      // cout<<"it_ull: "<<it_ull<<endl;

      if (it_ull >= end_key_ull) {
          break;
      }

      // Collect the key
      string key = it->key().ToString();
      // keys.push_back(key);

      // Collect the value
      string result = it->value().ToString();
      string value(valueSize, '\0');

      unsigned long long pos = 0;
      pos = stringToULL(result);
      memcpy(&pos, result.data(), sizeof(unsigned long long));
      // cout<<"pos: "<<pos<<endl;

      // char result_char[sizeof(unsigned long long)+1];
      // memset(result_char,0,sizeof(result_char));
      // strncpy(result_char,valueStr.c_str(),sizeof(unsigned long long));
      // pos = stoull(result_char);

      string vlog_path = VLOG_PATH;
      FILE * vlog = fopen (vlog_path.c_str(),"a+b");
      if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

      pos = pos*(keySize+valueSize);
      fseek(vlog,pos+keySize,SEEK_SET);
      fread(&value[0], valueSize, 1, vlog);
      //printf("value: %s",value);
      fclose(vlog);

      // values.push_back(value);
      kvs_lsm.push_back(make_pair(key, value));
      
      // cout<<"key: "<<key<<endl;
      // unsigned long long ull = stringToULL(key);
      // cout<<"key ull: "<<ull<<endl;
      // memcpy(&ull,key.c_str(),sizeof(unsigned long long));
      // cout<<"key ull: "<<ull<<endl;
      // cout<<"value: "<<value<<endl;
      // cout<<"value size: "<<value.size()<<endl;

    }

    if (!it->status().ok()) {
        std::cerr << "An error was found during the scan: " << it->status().ToString() << std::endl;
        delete it;
        return -1;
    }

    delete it;

    // if (kvs_lsm.size() == 0) {
    //     cout<<"start key: "<<key_ull<<endl;
    //     cout<<"end key: "<<end_key_ull<<endl;
    // }

    
    // part 1b: scan the range from Learned Index
    vector<pair<string,string>> kvs_learned;



    if(learned_index_used){
    
      // read the start key

      // lookup in linear models
      long long int pos = query_model(startKey, model_str);
      size_t kv_size = KEY_SIZE + VALUE_SIZE;
      long long int start_pos = max((pos - ERR_BOUND),0LL)*(kv_size);
      //start_pos = ceil((double)start_pos/kv_size) * kv_size; // round to multiple of kv_size
      long long int end_pos = (pos + ERR_BOUND)*(kv_size);
      //end_pos = end_pos/kv_size * kv_size; // round to multiple of kv_size


      // load key array chunk from key_array_path
      FILE * key_array_file = fopen(key_array_str.c_str(),"rb");
      // fseek(key_array_file, 0, SEEK_END);
      // size_t key_array_size = ftell(key_array_file);
      fseek(key_array_file, KEY_SIZE*(start_pos/kv_size), SEEK_SET);
      size_t key_array_chunk_size = KEY_SIZE*(end_pos/kv_size - start_pos/kv_size); // in bytes
      unsigned long long * key_array = new unsigned long long [key_array_chunk_size/sizeof(unsigned long long)];

      fread(key_array, key_array_chunk_size, 1, key_array_file);
      // fclose(key_array_file);

      size_t idx = 0;

      for (; idx < key_array_chunk_size/sizeof(unsigned long long); ++idx){
        if(key_array[idx]==startKey){
          break;
        }
      }
      start_pos = start_pos + idx * kv_size;

      long long int start_key_pos = start_pos;

      delete[] key_array;

      // read the end key
      // unsigned long long end_key_ull = key_ull + scanRange;
      pos = query_model(end_key_ull, model_str);
      start_pos = max((pos - ERR_BOUND),0LL)*(kv_size);
      end_pos = (pos + ERR_BOUND)*(kv_size);

      fseek(key_array_file, KEY_SIZE*(start_pos/kv_size), SEEK_SET);
      key_array_chunk_size = KEY_SIZE*(end_pos/kv_size - start_pos/kv_size); // in bytes
      key_array = new unsigned long long [key_array_chunk_size/sizeof(unsigned long long)];

      fread(key_array, key_array_chunk_size, 1, key_array_file);
      fclose(key_array_file);
      
      idx = 0;

      for (; idx < key_array_chunk_size/sizeof(unsigned long long); ++idx){
        if(key_array[idx]>=end_key_ull){
          break;
        }
      }
      start_pos = start_pos + idx * kv_size;
      long long int end_key_pos = start_pos;

      if(end_key_pos == start_key_pos){
        end_key_pos += kv_size;
      }

      delete[] key_array;


      // read from vlog
      string vlog_path = VLOG_PATH;
      FILE * vlog = fopen (vlog_path.c_str(),"r+b");
      if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

      char read_key[KEY_SIZE+1];
      char read_value[VALUE_SIZE + 1];
      // string read_key;
      // string read_value;
      
      // Clear the buffers
      memset(read_key, 0, sizeof(read_key));
      memset(read_value, 0, sizeof(read_value));

      fseek(vlog, start_key_pos, SEEK_SET);

      // cout<<"start pos: "<<start_key_pos<<endl;
      // cout<<"end pos: "<<end_key_pos<<endl;
      // cout<<"current pos: "<<ftell(vlog)<<endl;

      while (ftell(vlog) < end_key_pos) {
        // Read key
        unsigned long long key_ull = 0;
        fread(&key_ull, 1, KEY_SIZE, vlog);
        // read_key[KEY_SIZE] = '\0';  // Null-terminate the key string

        // Read value
        fread(read_value, 1, VALUE_SIZE, vlog);
        // read_value[VALUE_SIZE] = '\0';  // Null-terminate the value string

        // Add to vectors
        string key_str = ullToString(key_ull);
        kvs_learned.push_back(make_pair(key_str, string(read_value)));
        // auto key_ull = stringToULL(string(read_key));
        // cout<<"key: "<<key_ull<<endl;

        // keys.push_back(string(read_key));
        // values.push_back(string(read_value));

        // cout<<"key: "<<string(read_key)<<endl;
        // unsigned long long ull = 0;
        // memcpy(&ull,read_key,sizeof(unsigned long long));
        // cout<<"key ull: "<<ull<<endl;
        // cout<<"value: "<<string(read_value)<<endl;

        // Clear the buffers for the next read
        memset(read_key, 0, sizeof(read_key));
        memset(read_value, 0, sizeof(read_value));
        
      }
      // cout<<"kv learned: "<<kvs_learned.size()<<endl;
      // if(kvs_learned.size()==1){
      //   cout<<"start pos: "<<start_key_pos<<endl;
      //   cout<<"end pos: "<<end_key_pos<<endl;
      //   cout<<"start key: "<<key_ull<<endl;
      //   cout<<"end key: "<<end_key_ull<<endl;
      //   std::cout << "Maximum unsigned long long: " << ULLONG_MAX << std::endl;
      // }
    }

    // part 2: merge the results
    if (learned_index_used){
      // std::merge(kvs_lsm.begin(), kvs_lsm.end(), kvs_learned.begin(), kvs_learned.end(), std::back_inserter(kvs),
      //            [](const std::pair<std::string, std::string>& a, const std::pair<std::string, std::string>& b) {
      //              return stringToULL(a.first) < stringToULL(b.first);
      //            });
      // kvs = kvs_learned;
      // cout<<"kvs_lsm size: "<<kvs_lsm.size()<<endl;
      // cout<<"kvs_learned size: "<<kvs_learned.size()<<endl;
      mergeKeyValueVectors(kvs_lsm, kvs_learned, kvs);
    }
    else{
      // cout<<"kvs_lsm size: "<<kvs_lsm.size()<<endl;
      kvs = kvs_lsm;
    }

    scan_kvs_count += kvs.size();

    // cout<<"kvs size: "<<kvs.size()<<endl;

    return 0;
  }

  int initialize_new_rocksdb() {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.use_direct_reads = true;
    options.use_direct_io_for_flush_and_compaction = true;
    options.write_buffer_size = 64 * 1024;
    
    rocksdb::BlockBasedTableOptions table_options;
    table_options.block_cache = nullptr;
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));
    
    options.max_bytes_for_level_base = 16 * 1024;
    options.max_bytes_for_level_multiplier = 10;
    options.compression = rocksdb::kNoCompression;
    options.level_compaction_dynamic_level_bytes = false;

    rocksdb::Status status = rocksdb::DB::Open(options, "./db/db_new", &new_db);
    if (!status.ok()) {
      std::cerr << "Failed to open new database: " << status.ToString() << std::endl;
      return -1;
    }
    return 0;
  }

  // string create_new_vlog() {
  //   string new_vlog_path = string(NEW_VLOG_PATH);
  //   FILE* new_vlog = fopen(new_vlog_path.c_str(), "wb");
  //   if (new_vlog == NULL) {
  //       cerr << "Failed to create new vlog file" << endl;
  //       return "";
  //   }
  //   fclose(new_vlog);
  //   return new_vlog_path;
  // }

  void detroy_old_instance(){
    // destroy_rocksdb();
    string vlog_path = VLOG_PATH;
    FILE* vlog = fopen(vlog_path.c_str(), "rb");
    fseek(vlog, 0, SEEK_END);
    size_t file_size = ftell(vlog);
    total_num -= file_size / (KEY_SIZE + VALUE_SIZE);
    fclose(vlog);
    int res = std::remove(vlog_path.c_str());

    vlog_path = LI_VLOG_PATH;
    vlog = fopen(vlog_path.c_str(), "rb");
    if(vlog!=NULL){
      fseek(vlog, 0, SEEK_END);
      file_size = ftell(vlog);
      total_num -= file_size / (KEY_SIZE + VALUE_SIZE);
      fclose(vlog);
    }



    if (db != nullptr) {
        // delete db;
        db = new_db;  // Switch to the new DB
        new_db = nullptr;
        
        // // Clean up old DB files
        // rocksdb::Options options;
        // rocksdb::Status status = rocksdb::DestroyDB("./db/db", options);
        // if (!status.ok()) {
        //     std::cerr << "Failed to erase old database: " << status.ToString() << std::endl;
        // }
        
        // Rename new DB directory to replace old one
        rename("./db/db_new", "./db/db");
    }

    string old_vlog = LI_VLOG_PATH;
    string new_vlog = NEW_LI_VLOG_PATH;
    
    if (rename(new_vlog.c_str(), old_vlog.c_str()) != 0) {
        cerr << "Error renaming new vlog file" << endl;
    }

    old_vlog = VLOG_PATH;
    new_vlog = NEW_VLOG_PATH;
    if (rename(new_vlog.c_str(), old_vlog.c_str()) != 0) {
        cerr << "Error renaming new vlog file" << endl;
    }

    string model_s = model_str;
    res = std::remove(model_s.c_str());
    if (rename(new_model_str.c_str(), model_str.c_str()) != 0) {
        cerr << "Error renaming new model file" << endl;
    }

    res = std::remove(key_array_str.c_str());
    if (rename(new_key_array_str.c_str(), key_array_str.c_str()) != 0) {
        cerr << "Error renaming new model file" << endl;
    }

    gc_in_progress = false;
  }

};
