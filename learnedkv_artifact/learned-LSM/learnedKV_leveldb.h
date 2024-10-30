#include <iostream>
// #include <rocksdb/db.h>
// #include <rocksdb/table.h>
#include <leveldb/db.h>
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
  leveldb::DB* db; // for key-ptr index
  //char* vlog; // points to the vlog file; can be cast to FILE*

  //KEY_TYPE pivot;

  // PLR models m;
  //vector<Segment> model;
  string model_str; // the path of the model file

  // record insert position of vlog // presented by the num of kv that have been already inserted
  unsigned long long write_pos;

  //KeyPtr* key_arry; //Can be optimized later
  //vector<unsigned long long> key_array;
  string key_array_str; // the path of the key_array file

  BloomFilter filter{100000000, 7}; // 100,000,000 bits, 7 hash functions

  LearnedKV(){
    initialize_rocksdb();
    write_pos=0;
    model_str = "./db/model";
    key_array_str = "./db/key_array";
  }

  ~LearnedKV(){
    // Close the database
    delete db;
    // TODO: clean up the model and key_arry files
  }

  int initialize_rocksdb(){
    // rocksdb initialization

    leveldb::Options options;
    options.create_if_missing = true;

    // options.write_buffer_size = 64 *  1024; // memtable size = 64KB

    // // disable write cache
    // rocksdb::BlockBasedTableOptions table_options;
    // table_options.block_cache = nullptr;
    // options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

    // // Set the maximum total data size for level-1
    // options.max_bytes_for_level_base = 16 * 1024; // 16KB

    // // Set the size ratio between levels
    // options.max_bytes_for_level_multiplier = 10;

    // // // disable compression
    // // options.compression = rocksdb::kNoCompression;

    // // // diable level compaction
    // // options.level_compaction_dynamic_level_bytes = false;


    // for leveldb
    //mod
    options.write_buffer_size = 16 * 1024; // 64KB
    options.max_file_size = 1 * 1024; // 16KB // modified in version_set
    options.compression = leveldb::CompressionType::kNoCompression;


    leveldb::Status status = leveldb::DB::Open(options, "./db/db", &db);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString() << std::endl;
      return -1;
    }

    filter.reset();

    return 0;
  }

  int destroy_rocksdb(){
    // Close the database.
    delete db;
    // Erase the database directory.
    leveldb::Options options;
    leveldb::Status status = leveldb::DestroyDB("./db/db", options);
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
    //model.clear();
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
    FILE * model_file = fopen(model_str.c_str(),"wb");
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
    FILE * key_array_file = fopen(key_array_str.c_str(),"wb");
    if(key_array_file==NULL){
      cout<<"key array file does not exist."<<endl;
      return -1;
    }
    fwrite(x.data(),x.size()*sizeof(unsigned long long),1,key_array_file);
    // for(size_t i = 0;i<x.size();i++){
    //   fwrite(&x[i],sizeof(unsigned long long),1,key_array_file);
    // }
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
    if (pos<0){return 0;}
    return pos;
  }

  int put(char *key, uint64_t keySize, char *value, uint64_t valueSize){
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
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog, 0, SEEK_END);
    assert(write_pos*(keySize+valueSize) == (unsigned long long)ftell(vlog));
    //write_pos = (uint64_t)ftell(vlog);
    //cout<<g->write_pos<<endl;
    fwrite(key,keySize,1,vlog);
    fwrite(value,valueSize,1,vlog);
    fclose(vlog);
    
    char ptr[sizeof(unsigned long long)+1];
    //unsigned char * ptr = new unsigned char [sizeof(uint64_t)+1];
    memset(ptr,0,sizeof(ptr));
    assert(write_pos<100000000ULL);// write_pos should be smaller than 10^8
    memcpy(ptr, to_string(write_pos).c_str(), sizeof(unsigned long long));

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,key,sizeof(unsigned long long));
    // if(key_ull==9736846280066027119ULL){
    //   cout<<"ready to put."<<endl;
    //   cout<<"key: "<<key_ull<<endl;
    //   unsigned char *pt = reinterpret_cast<unsigned char*>(&write_pos);
    //   for (size_t i = 0; i < sizeof(write_pos); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(pt[i]) << ' ';
    //   }
    //   cout<<hex<<"write_pos: "<<write_pos<<endl;
    //   for (size_t i = 0; i < sizeof(another_ptr); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(another_ptr[i]) << ' ';
    //   }

    //   //cout<<"ptr: "<<ptr[0]<<endl;
    //   // unsigned long long new_ull = 0;
    //   // memcpy(&new_ull, ptr, sizeof(unsigned long long));
    //   // cout<<"new ull: "<<new_ull<<endl;
    //   //cout<<hex<<"ptr: "<<ptr[0]<<"|"<<ptr[1]<<"|"<<ptr[2]<<"|"<<ptr[3]<<"|"<<ptr[4]<<"|"<<ptr[5]<<"|"<<ptr[6]<<"|"<<ptr[7]<<"|"<<endl;
    // }

    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, ptr);
    if (!status.ok()) {
        std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
        return -1;
    }
    
    //write_pos += keySize + valueSize;
    write_pos ++;

    unsigned long long key_ull = 0;
    memcpy(&key_ull,key,sizeof(unsigned long long));
    filter.insert(key_ull);

    return 0;
  }


  int get(char *key, uint64_t keySize, char *value, uint64_t valueSize){
    //char *result =(char*) malloc(1025);;
    //memset(result,0,1025);
    bool accurate = true;
    string result;
    bool rocksdb_found = false;

    // struct timeval startTime, endTime;
    // gettimeofday(&startTime, 0);

    unsigned long long key_ull = 0;
    memcpy(&key_ull,key,sizeof(unsigned long long));
    if (filter.possiblyContains(key_ull)){
      struct timeval startTime, endTime;
      gettimeofday(&startTime, 0);

      // static int count = 0;  // Counts the number of times the function is called
      // static std::chrono::nanoseconds totalDuration(0);  // Accumulates total duration
      // auto start = std::chrono::high_resolution_clock::now();  // Start timer 

      leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &result);

      // auto end = std::chrono::high_resolution_clock::now();  // End timer
      // std::chrono::nanoseconds duration = end - start;
      // totalDuration += duration;  // Accumulate total time
      // ++count;  // Increment call count
      // // Optionally, print average time every N calls
      // if (count % 1000 == 0) {
      //     double averageTime = static_cast<double>(totalDuration.count()) / count;
      //     std::cout << "Average time per call after " << count << " calls: " 
      //               << averageTime << " nanoseconds" << std::endl;
      // }

      gettimeofday(&endTime, 0);
      rocksdb_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
      rocksdb_count++;
      if (status.ok()){
        rocksdb_found = true;
      }
    }


    // gettimeofday(&endTime, 0);
    // rocksdb_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // if(!status.ok()){
    //   rocksdb_fail_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // }
    // else{
    //   rocksdb_success_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // }
    
    if (!rocksdb_found) {
      //std::cerr << "Failed to read data from database: " << status.ToString() << std::endl;
      
      if(learned_index_used){
        struct timeval startTime, endTime;
        gettimeofday(&startTime, 0);


        //look up in linear models
        
        accurate = false;
        unsigned long long key_ull = 0;
        memcpy(&key_ull,key,sizeof(unsigned long long));
        long long int pos = query_model(key_ull, model_str);
        size_t kv_size = KEY_SIZE + VALUE_SIZE;
        long long int start_pos = max((pos - ERR_BOUND),0LL)*(kv_size);
        //start_pos = ceil((double)start_pos/kv_size) * kv_size; // round to multiple of kv_size
        long long int end_pos = (pos + ERR_BOUND)*(kv_size);
        //end_pos = end_pos/kv_size * kv_size; // round to multiple of kv_size
        
        //fseek(vlog,start_pos,SEEK_SET);

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
          if(key_array[idx]==key_ull){
            //cout<<"found key in key array."<<endl;
            //accurate = true;
            break;
          }
        }
        start_pos = start_pos + idx * kv_size;

        delete[] key_array;
        
        // while (start_pos<=end_pos){
        //   // method 1
        //   int start_idx = start_pos/kv_size;
        //   if(key_array[start_idx]==key_ull){
        //     break;
        //   }
        //   start_pos += kv_size;

        //   // method 2
        //   // memset(read_key,0,sizeof(read_key));
        //   // int res = fread(read_key, keySize, 1, vlog);
        //   // if(res<0){cout<<"Failed to read."<<endl;return -1;}
        //   // if(memcmp(key,read_key,keySize)==0){
        //   //   fread(value, valueSize, 1, vlog);
        //   //   break;
        //   // }
        //   // start_pos += kv_size;
        //   // fseek(vlog,VALUE_SIZE,SEEK_CUR);
        // }


        gettimeofday(&endTime, 0);
        learned_index_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
        learned_index_count++;

        struct timeval load_startTime, load_endTime;
        gettimeofday(&load_startTime, 0);

        string vlog_path = VLOG_PATH;
        FILE * vlog = fopen (vlog_path.c_str(),"a+b");
        if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

        char read_key[KEY_SIZE+1];
        fseek(vlog,start_pos,SEEK_SET);
        fread(read_key, keySize, 1, vlog);
        fread(value, valueSize, 1, vlog);

        //printf("value: %s",value);
        fclose(vlog);

        gettimeofday(&load_endTime, 0);

        load_kv_time += (load_endTime.tv_sec - load_startTime.tv_sec) * 1000000 + (load_endTime.tv_usec - load_startTime.tv_usec);
        load_kv_count++;

        

        return 0;
      }
      else{
        std::cerr << "Failed to read data from database: " << std::endl;
        unsigned long long key_ull = 0;
        memcpy(&key_ull,key,sizeof(unsigned long long));
        cout<<"Missed key: "<<key_ull<<endl;
        return -1;
      }

    }
    
    struct timeval startTime, endTime;
    gettimeofday(&startTime, 0);

    unsigned long long pos = 0;

    char result_char[sizeof(unsigned long long)+1];
    memset(result_char,0,sizeof(result_char));
    strncpy(result_char,result.c_str(),sizeof(unsigned long long));
    //memcpy(&pos,result_char,sizeof(uint64_t));
    pos = stoull(result_char);

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,key,sizeof(unsigned long long));
    // if(key_ull==9736846280066027119ULL){
    //   cout<<"rocksdb result (char*) to write pos (int)."<<endl;
    //   cout<<"result: "<<result<<endl;
    //   cout<<"size of result: "<<sizeof(result.c_str())<<endl;
    //   cout<<hex<<"write_pos: "<<pos<<endl;
    //   //cout<<"result_char: "<<result_char[0]<<endl;
    //   for (size_t i = 0; i < sizeof(result_char); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result_char[i]) << ' ';
    //   }
    //   // char key_char[sizeof(uint64_t)+1];
    //   // memcpy(key_char,result.c_str(),sizeof(result.c_str()));
    //   // cout<<"key char: "<<key_char<<endl;
    //   // uint64_t temp = 0;
    //   // memcpy(&temp,key_char,sizeof(uint64_t));
    //   // cout<<"temp: "<<temp<<endl;
    // }


    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}
    //cout<<g->write_pos<<endl;
    pos = pos*(keySize+valueSize);
    fseek(vlog,pos+keySize,SEEK_SET);
    fread(value, valueSize, 1, vlog);
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
    

    // // Retrieve and display column family metadata
    // rocksdb::ColumnFamilyMetaData cf_meta;
    // db->GetColumnFamilyMetaData(&cf_meta);

    // std::cout << "Column Family Name: " << cf_meta.name << std::endl;
    // std::cout << "Size (bytes): " << cf_meta.size << std::endl;
    // std::cout << "Number of files: " << cf_meta.file_count << std::endl;
    // std::cout << "Number of levels: " << cf_meta.levels.size() << std::endl;

    // for (const auto& level : cf_meta.levels) {
    //     std::cout << " Level " << level.level << ":\n";
    //     std::cout << "  Number of files: " << level.files.size() << std::endl;
    //     for (const auto& file : level.files) {
    //         std::cout << "  File number: " << file.file_number << ", Size: " << file.size << " bytes" << std::endl;
    //     }
    // }


    // TODO: collect valid data and re-build LSM
    unordered_map<unsigned long long, string> valid_map;
    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

    fseek(vlog,0,SEEK_SET);
    char key[KEY_SIZE+1];
    char value[VALUE_SIZE+1];
    memset(key, 0, sizeof(key));
    memset(value, 0, sizeof(value));
    while (fread(key, sizeof(char), KEY_SIZE, vlog)>0) { // loop until end of file
      size_t bytes_read=0;
      bytes_read = fread(value, sizeof(char), VALUE_SIZE, vlog);
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



    // erase rocksdb
    destroy_rocksdb();
    //std::cout << "Database erased successfully." << std::endl;
    // Set up LSM options
    initialize_rocksdb();
    //std::cout << "Database set up successfully." << std::endl;
    

    // rewrite the vlog file
    vlog = fopen (vlog_path.c_str(),"wb");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;
    
    vector<pair<unsigned long long, string>> valid_vec(valid_map.begin(), valid_map.end()); // key -> value

    // Sort the vector based on the keys
    sort(valid_vec.begin(), valid_vec.end());

    vector<unsigned long long> x; x.clear();
    vector<double> y; y.clear();

    // leveldb::WriteBatch batch;
    char ptr[sizeof(unsigned long long)+1];
    char key_char[KEY_SIZE+1];

    for (auto valid_kv:valid_vec){
      unsigned long long key_ull = valid_kv.first;
      //cout<<"key_str: "<<key_str<<endl;
      string value_str = valid_kv.second;

      
      memset(key_char,0,sizeof(key_char));
      memcpy(key_char,&key_ull,sizeof(unsigned long long));

      // write to vlog
      fwrite(key_char,keySize,1,vlog);
      fwrite(value_str.c_str(),valueSize,1,vlog);

      if(learned_index_used){
        x.push_back(key_ull);
        //cout<<"ull: "<<ull<<endl;
        y.push_back((double)write_pos);
      }

      // write to rocksdb
      if(!learned_index_used){
        
        memset(ptr,0,sizeof(ptr));
        memcpy(ptr,to_string(write_pos).c_str(),sizeof(unsigned long long));
        
        // // enable batch write
        // batch.Put(key_char, ptr);

        filter.insert(key_ull);

        leveldb::Status status = db->Put(leveldb::WriteOptions(), key_char, ptr);
        if (!status.ok()) {
            std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
            return -1;
        }
      }
      //write_pos += keySize + valueSize;
      write_pos++;
    }
    //key_array=x;

    // if(!learned_index_used){
    //   leveldb::Status status = db->Write(rocksdb::WriteOptions(), &batch);
    //   if (!status.ok()) {
    //       std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
    //       return -1;
    //   }

    //   // flush the database, otherwise all the batch is in memory
    //   leveldb::FlushOptions flush_options;
    //   flush_options.wait = true;  // Wait for the flush to complete

    //   status = db->Flush(flush_options);
    //   if (!status.ok()) {
    //       std::cerr << "Failed to flush the database: " << status.ToString() << std::endl;
    //   }

    // }

    // TODO: build learned index
    if(learned_index_used){
      build_piecewise_linear_regression(x, y, ERR_BOUND);
    }

    gc_write_size += (KEY_SIZE+VALUE_SIZE)*valid_vec.size();


    fclose(vlog);
    return 0;
  }


};
