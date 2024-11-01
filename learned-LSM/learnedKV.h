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

using namespace std;

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

    rocksdb::Options options;
    options.create_if_missing = true;

    // Enable direct I/O
    options.use_direct_reads = true;
    options.use_direct_io_for_flush_and_compaction = true;

    options.write_buffer_size = 64 *  1024; // memtable size = 64KB

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
    FILE * model_file = fopen(model_str.c_str(),"wb");
    if(model_file==NULL){
      cout<<"model file does not exist."<<endl;
      return -1;
    }
    fwrite(model.data(),model.size()*sizeof(Segment),1,model_file);
    fclose(model_file);

    // write key array to file
    FILE * key_array_file = fopen(key_array_str.c_str(),"wb");
    if(key_array_file==NULL){
      cout<<"key array file does not exist."<<endl;
      return -1;
    }
    fwrite(x.data(),x.size()*sizeof(unsigned long long),1,key_array_file);

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
    // check if it can be correctly inserted

    if((write_pos+1)*(keySize+valueSize) > group_size){
      garbage_collection(keySize, valueSize);
      gc_count++;
    }
    assert((write_pos+1)*(keySize+valueSize) <= group_size);
    
    
    //insert K,V in vlog (file)
    //and insert K,P in LSM (buffer, then SST files)

    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    // FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "ab");
    fwrite(&key,keySize,1,vlog);
    fwrite(value.data(),valueSize,1,vlog);
    // DirectIOHelper::writeAligned(vlog, &key, keySize);
    // DirectIOHelper::writeAligned(vlog, value.data(), valueSize);
    fclose(vlog);
    

    assert(write_pos<100000000ULL);// write_pos should be smaller than 10^8
    string ptr = ullToString(write_pos);

    string key_str = ullToString(key);
    rocksdb::Status status = db->Put(rocksdb::WriteOptions(), key_str, ptr);
    if (!status.ok()) {
        std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
        return -1;
    }
    
    //write_pos += keySize + valueSize;
    write_pos ++;

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,key,sizeof(unsigned long long));
    filter.insert(key);


    return 0;
  }


  int get(unsigned long long key, uint64_t keySize, string& value, uint64_t valueSize){
    bool accurate = true;
    string result;
    bool rocksdb_found = false;

    // struct timeval startTime, endTime;
    // gettimeofday(&startTime, 0);

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,key,sizeof(unsigned long long));
    if (filter.possiblyContains(key)){
      // struct timeval startTime_rocksdb, endTime_rocksdb;
      // gettimeofday(&startTime_rocksdb, 0);

      // static int count = 0;  // Counts the number of times the function is called
      // static std::chrono::nanoseconds totalDuration(0);  // Accumulates total duration
      // auto start = std::chrono::high_resolution_clock::now();  // Start timer 

      string key_str = ullToString(key);
      rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key_str, &result);

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


      // gettimeofday(&endTime_rocksdb, 0);
      // rocksdb_time += (endTime_rocksdb.tv_sec - startTime_rocksdb.tv_sec) * 1000000 + (endTime_rocksdb.tv_usec - startTime_rocksdb.tv_usec);
      rocksdb_count++;
      if (status.ok()){
        rocksdb_found = true;
      }
    }

    // // only used to time beakdown test
    // gettimeofday(&endTime, 0);
    // // rocksdb_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // if(!rocksdb_found){
    //   rocksdb_fail_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // }
    // else{
    //   rocksdb_success_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    // }
    
    if (!rocksdb_found) {
      
      if(learned_index_used){
        // struct timeval startTime_learned_index, endTime_learned_index;
        // gettimeofday(&startTime_learned_index, 0);


        //look up in linear models
        
        accurate = false;
        // unsigned long long key_ull = 0;
        // memcpy(&key_ull,key,sizeof(unsigned long long));
        long long int pos = query_model(key, model_str);
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
          if(key_array[idx]==key){
            //cout<<"found key in key array."<<endl;
            //accurate = true;
            break;
          }
        }
        start_pos = start_pos + idx * kv_size;

        delete[] key_array;

        // gettimeofday(&endTime_learned_index, 0);
        // learned_index_time += (endTime_learned_index.tv_sec - startTime_learned_index.tv_sec) * 1000000 + (endTime_learned_index.tv_usec - startTime_learned_index.tv_usec);
        learned_index_count++;

        // struct timeval load_startTime, load_endTime;
        // gettimeofday(&load_startTime, 0);

        string vlog_path = VLOG_PATH;
        FILE * vlog = fopen (vlog_path.c_str(),"r+b");
        // FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "rb");
        if(vlog==NULL){cout<<"Cannot open the file"<<endl;}


        char read_key[KEY_SIZE+1];
        fseek(vlog,start_pos,SEEK_SET);
        fread(read_key, keySize, 1, vlog);
        unsigned long long read_key_ull = 0;
        memcpy(&read_key_ull,read_key,sizeof(unsigned long long));
        
        value.resize(valueSize);
        fread(&value[0], valueSize, 1, vlog);

        // char read_key[KEY_SIZE];
        // DirectIOHelper::readAligned(vlog, read_key, keySize);
        // value.resize(valueSize);
        // DirectIOHelper::readAligned(vlog, &value[0], valueSize);
        fclose(vlog);

        // gettimeofday(&load_endTime, 0);

        // load_kv_time += (load_endTime.tv_sec - load_startTime.tv_sec) * 1000000 + (load_endTime.tv_usec - load_startTime.tv_usec);
        load_kv_count++;

        

        return 0;
      }
      else{
        // std::cerr << "Failed to read data from database: " << std::endl;
        return -1;
      }

    }
    
    // struct timeval startTime_load, endTime_load;
    // gettimeofday(&startTime_load, 0);

    unsigned long long pos = 0;


    pos = stringToULL(result);


    string vlog_path = VLOG_PATH;
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


    // collect valid data and re-build LSM
    unordered_map<unsigned long long, string> valid_map;
    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"rb");
    // FILE* vlog = DirectIOHelper::openFile(vlog_path.c_str(), "rb");
    if(vlog==NULL){cout<<"Cannot open the file"<<endl;}

    fseek(vlog,0,SEEK_SET);
    char key[KEY_SIZE+1];
    // char value[VALUE_SIZE+1];
    string value;
    memset(key, 0, sizeof(key));
    // memset(value, 0, sizeof(value));
    while (fread(key, sizeof(char), KEY_SIZE, vlog)>0) { // loop until end of file
    // while (DirectIOHelper::readAligned(vlog, key, KEY_SIZE)) {
      size_t bytes_read=0;
      value.resize(VALUE_SIZE);
      bytes_read = fread(&value[0], sizeof(char), VALUE_SIZE, vlog);
      //cout<<"bytes read for value: "<<bytes_read<<endl;
      assert(bytes_read==VALUE_SIZE);

      // value.resize(VALUE_SIZE);
      // DirectIOHelper::readAligned(vlog, &value[0], VALUE_SIZE);

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
    vlog = fopen (vlog_path.c_str(),"a+b");
    // vlog = DirectIOHelper::openFile(vlog_path.c_str(), "wb");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;
    
    vector<pair<unsigned long long, string>> valid_vec(valid_map.begin(), valid_map.end()); // key -> value

    // Sort the vector based on the keys
    sort(valid_vec.begin(), valid_vec.end());

    vector<unsigned long long> x; x.clear();
    vector<double> y; y.clear();

    rocksdb::WriteBatch batch;
    string ptr;
    
    for (auto it = valid_vec.begin(); it != valid_vec.end() - 1; ++it){
      
      auto valid_kv = *it;


      unsigned long long key_ull = valid_kv.first;

      string value_str = valid_kv.second;

      
      // memset(key_char,0,sizeof(key_char));
      // memcpy(key_char,&key_ull,sizeof(unsigned long long));

      // write to vlog
      int num = fwrite(&key_ull,keySize,1,vlog);
      // if(num!=1){cout<<"Failed to write key to vlog."<<endl;}
      num = fwrite(valid_kv.second.data(),valueSize,1,vlog);
      // if(num!=1){cout<<"Failed to write value to vlog."<<endl;}
      

      // DirectIOHelper::writeAligned(vlog, &key_ull, keySize);
      // DirectIOHelper::writeAligned(vlog, valid_kv.second.data(), valueSize);


      if(learned_index_used){
        x.push_back(key_ull);
        y.push_back((double)write_pos);
      }

      // write to rocksdb
      if(!learned_index_used){
        ptr = ullToString(write_pos);
        
        string key_str = ullToString(key_ull);
        // enable batch write
        batch.Put(key_str, ptr);

        filter.insert(key_ull);

      }
      //write_pos += keySize + valueSize;
      write_pos++;
    }
    //key_array=x;

    if(!learned_index_used){
      rocksdb::Status status = db->Write(rocksdb::WriteOptions(), &batch);
      if (!status.ok()) {
          std::cerr << "Failed to write data to database: " << status.ToString() << std::endl;
          return -1;
      }

      // flush the database, otherwise all the batch is in memory
      rocksdb::FlushOptions flush_options;
      flush_options.wait = true;  // Wait for the flush to complete

      status = db->Flush(flush_options);
      if (!status.ok()) {
          std::cerr << "Failed to flush the database: " << status.ToString() << std::endl;
      }

      // // only for collecting statistics
      // bytes_written += 16*valid_vec.size();

    }

    // build learned index
    if(learned_index_used){
      build_piecewise_linear_regression(x, y, ERR_BOUND);

      // // only for collecting statistics
      // bytes_written_learned_index += (8)*valid_vec.size();
    }

    gc_write_size += (KEY_SIZE+VALUE_SIZE)*valid_vec.size();


    fclose(vlog);
    return 0;
  }


  // keys,values,valueSizes should be empty before calling this function
  int scan(unsigned long long startKey, uint64_t keySize, vector<pair<string,string>>& kvs, uint64_t valueSize, unsigned long long scanRange){

    
    // part 1: scan the range from both LSM and Learned Index
    // part 2: merge the results

    vector<pair<string,string>> kvs_lsm;


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
      

    }

    if (!it->status().ok()) {
        std::cerr << "An error was found during the scan: " << it->status().ToString() << std::endl;
        delete it;
        return -1;
    }

    delete it;


    
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

        // Clear the buffers for the next read
        memset(read_key, 0, sizeof(read_key));
        memset(read_value, 0, sizeof(read_value));
        
      }
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

};
