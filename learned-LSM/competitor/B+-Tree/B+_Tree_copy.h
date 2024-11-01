#include <iostream>
// #include <rocksdb/db.h>
// #include <rocksdb/table.h>
#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include "../../utils.h"
#include <arpa/inet.h>
// #include "./b+_tree/b_tree.h"
#include "btree-disk.h"

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

extern long scan_kvs_count;

extern unsigned long long group_size;


class LearnedKV {
  public:

  // B+ Tree
  BaselineBTreeDisk* bt;


  // record insert position of vlog // presented by the num of kv that have been already inserted
  unsigned long long write_pos;


  LearnedKV(){
    initialize_rocksdb();
    write_pos=0;

  }

  ~LearnedKV(){
    // Close the database
    // delete db;
    delete bt;
    // TODO: clean up the model and key_arry files
  }

  void bulk_load (vector<pair<string, unsigned long long>> &workloads , unsigned int num_kv){
    // bulk load
    // char* index_name = "index_name";
    // bt = new BTree(0, true, index_name, true);
    // LeafNodeIterm *data = new LeafNodeIterm[num_kv];
    vector<pair<uint64_t, uint64_t>> key_value;
    
    // rewrite the vlog file
    string vlog_path = VLOG_PATH;
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;

    for (int i=0;i<num_kv;i++){


      // write to vlog
      unsigned long long key_ull = workloads[i].second;
      int num = fwrite(&key_ull,KEY_SIZE,1,vlog);

      char key_char = static_cast<char>(key_ull & 0xFF);
      string value;
      // Resize the string to the desired size
      value.resize(VALUE_SIZE);

      // Fill the string with the character derived from key
      memset(&value[0], key_char, VALUE_SIZE);
      
      num = fwrite(value.data(),VALUE_SIZE,1,vlog);

      // data[i].key = workloads[i].second;
      // data[i].value = write_pos;

      key_value.push_back(make_pair(workloads[i].second, write_pos));

      write_pos++;
    }
    // bt->bulk_load(data, num_kv);
    bt->Build(key_value);

    fclose(vlog);
  }

  int initialize_rocksdb(){

    //bulk load
    // char* index_name = "index_name";
    // bt = new BTree(0, true, index_name, true);
    // LeafNodeIterm *data = new LeafNodeIterm[KVNUM];
    bt = new BaselineBTreeDisk();

    return 0;
  }

  int destroy_rocksdb(){
    delete bt;
    int res = std::remove("btree_disk");
    
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
    FILE * vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog, 0, SEEK_END);
    assert(write_pos*(keySize+valueSize) == (unsigned long long)ftell(vlog));
    //write_pos = (uint64_t)ftell(vlog);
    //cout<<g->write_pos<<endl;
    fwrite(&key,keySize,1,vlog);
    fwrite(value.data(),valueSize,1,vlog);
    fclose(vlog);
    


    // string key_str = ullToString(key);
    // string ptr = ullToString(write_pos);
    
    // db->Put(rocksdb::WriteOptions(), key_str, ptr);
    // bt->insert_key_entry(key, write_pos, &sel, &sml, &inl, &smo_c, &update_c);
    // bt->sync_metanode();
    bt->Insert(key, write_pos);

    // if(key == 9736846280066027119ULL){
    //   cout<<"key: "<<key<<endl;
    //   cout<<"write_pos: "<<write_pos<<endl;
    //   cout<<"sel: "<<sel<<endl;
    //   cout<<"sml: "<<sml<<endl;
    //   cout<<"inl: "<<inl<<endl;
    //   cout<<"smo_c: "<<smo_c<<endl;
    //   cout<<"update_c: "<<update_c<<endl;
    // }



    assert(write_pos<100000000ULL);// write_pos should be smaller than 10^8

    //write_pos += keySize + valueSize;
    write_pos ++;

    // unsigned long long key_ull = 0;
    // memcpy(&key_ull,key,sizeof(unsigned long long));
    // filter.insert(key);

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

    unsigned long long pos = bt->Find(key);

    // string key_str = ullToString(key);
    // rocksdb::Status status = db->Get(rocksdb::ReadOptions(), key_str, &result);

    struct timeval startTime, endTime;
    gettimeofday(&startTime, 0);

    // unsigned long long pos = c;


    // pos = stringToULL(result);




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

    gettimeofday(&endTime, 0);
    // //rocksdb_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    load_kv_time += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
    //rocksdb_count++;
    load_kv_count++;

    return 0;
  }

  int garbage_collection(uint64_t keySize, uint64_t valueSize){
    


    // TODO: collect valid data and re-build LSM
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



    // erase rocksdb
    destroy_rocksdb();
    //std::cout << "Database erased successfully." << std::endl;

    int res = std::remove(vlog_path.c_str());

    // Set up LSM options
    initialize_rocksdb();
    //std::cout << "Database set up successfully." << std::endl;
    

    // rewrite the vlog file
    vlog = fopen (vlog_path.c_str(),"a+b");
    fseek(vlog,0,SEEK_SET);
    write_pos = 0;
    
    vector<pair<unsigned long long, string>> valid_vec(valid_map.begin(), valid_map.end()); // key -> value

    // Sort the vector based on the keys
    sort(valid_vec.begin(), valid_vec.end());


    vector<pair<unsigned long, unsigned long>> key_value;

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


      // bt->Insert(key_ull, write_pos);
      key_value.push_back(make_pair(key_ull, write_pos));

      // bt->insert_key_entry(key_ull, write_pos, &sel, &sml, &inl, &smo_c, &update_c);
      // bt->sync_metanode();


      // filter.insert(key_ull);


      //write_pos += keySize + valueSize;
      write_pos++;
    }
    //key_array=x;

    bt->Build(key_value);

    gc_write_size += (KEY_SIZE+VALUE_SIZE)*valid_vec.size();


    fclose(vlog);
    return 0;
  }


};
