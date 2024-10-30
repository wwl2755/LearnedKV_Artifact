#include <iostream>
#include <rocksdb/db.h>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <ctime>
#include <climits>
#include <chrono>
#include <stdlib.h>     // srand(), rand()
#include <functional>
#include <sys/time.h>  // for gettimeofday()
#include <random>
#include <unistd.h>
#include <algorithm>
#include "b_tree.h"
#include "util.h"
#include "utils.h"

#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 8 // KV size is 1024 bytes

using namespace std;

unsigned long long scanRange = 10000000000000000ULL; // 1*10^16

// group config
int numOfGroup = 1;
bool hash_based = false; // True: KV pairs are separated by hash values; False: Separated by key range

bool learned_index_used = true; 

unsigned int KVNUM;
unordered_map<string, long> time_map;

long learned_index_time = 0;
long rocksdb_time = 0;
long rocksdb_success_time = 0;
long rocksdb_fail_time = 0;
long learned_index_count = 0;
long rocksdb_count = 0;
long load_kv_time = 0;
long load_kv_count = 0;
long gc_count = 0;
long gc_write_size = 0;

long scan_kvs_count = 0;

string output_path = "./out.csv";

unsigned long long group_size = 0;


// void gen_value(char* key, char* value, size_t size){
//     memset(value,key[0],size);
// }
void gen_value(unsigned long long key, std::string& value, size_t size) {
    // Convert the ULL to a character (e.g., use the lowest byte)
    char key_char = static_cast<char>(key & 0xFF);

    // Resize the string to the desired size
    value.resize(size);

    // Fill the string with the character derived from key
    memset(&value[0], key_char, size);
}
bool file_exists(const std::string &str) {
    std::ifstream fs(str);
    return fs.is_open();
}




// imported from https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
unsigned long long hash_ull(unsigned long long x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

void read_workload(vector<pair<string,unsigned long long>>& workloads, string file_path){ // pass by reference to avoid copying
    string line;
    ifstream myfile(file_path.c_str());
    int count = 0;
    if (myfile.is_open())
    {
        cout<<"Open the file: "<<file_path<<endl;
        while ( getline (myfile,line))
        {

            if(line =="")break;
            string delim = " key: ";
            string op = line.substr(0,line.find(delim)); // INSERT, READ, UPDATE, SCAN, DELETE
            string read_key = line.erase(0, line.find(delim) + delim.length() + 4); // delim "user"
            workloads.push_back(make_pair(op, stoull(read_key)));
            count++;
        }
        myfile.close();
    }
    else {
        std::cout << "Unable to open file";
        exit(-1);
    }
    cout<<"Total num of workloads: "<<workloads.size()<<endl;
}


void print_throughtput(long &write_count, long &read_count){
    cout<<"-----Throughput (KOPS)----------"<<endl;
    cout<<"Num of write operations: "<<write_count<<endl;
    cout<<"Throughput: "<<1000 * (double)(write_count)/time_map["UPDATE"]<<endl;
    cout<<"Num of read operations: "<<read_count<<endl;
    cout<<"Throughput: "<<1000 * (double)read_count/time_map["READ"]<<endl;
    if (learned_index_used){
        cout<<"Read query through Learned index: "<<learned_index_count<<endl;
        cout<<"Throughput: "<<1000* (double)learned_index_count/learned_index_time<<endl;
    }
    cout<<"rocksdb throughput when fail: "<<1000* (double)learned_index_count/rocksdb_fail_time<<endl;
    cout<<"rocksdb throughput when success: "<<1000* (double)(read_count-learned_index_count)/rocksdb_success_time<<endl;
    cout<<"GC num: "<<gc_count<<endl;
    cout<<"GC write bytes: "<<gc_write_size<<endl;

    std::ofstream ofile;
    ofile.open(output_path, std::ios::app);
    ofile << write_count << ",";
    ofile << read_count << ",";
    ofile << learned_index_count << ",";
    ofile << 1000 * (double)(write_count)/time_map["UPDATE"] << ",";
    ofile << 1000 * (double)read_count/time_map["READ"] << ",";

    read_count = 0;
    write_count = 0;
    time_map.clear();
    learned_index_count=0;
    learned_index_time=0;
    rocksdb_fail_time=0;
    rocksdb_success_time=0;
    gc_count=0;
    gc_write_size=0;
}

void print_throughtput_for_scan(long& scan_count){
    cout<<"-----Throughput (KOPS)----------"<<endl;
    cout<<"Num of scan operations: "<<scan_count<<endl;
    cout<<"Throughput: "<<1000 * (double)scan_count/time_map["SCAN"]<<endl;
    cout<<"Num of kvs scanned: "<<scan_kvs_count<<endl;
    scan_count = 0;
    time_map.clear();
}

void print_time_breakdown(long &write_count, long &read_count){
    cout<<"-----Time breakdown (ms) ----------"<<endl;
    cout<<"Num of write operations: "<<write_count<<endl;
    cout<<"Time cost: "<<(double) (time_map["UPDATE"])/(write_count)<<endl;
    cout<<"Num of read operations: "<<read_count<<endl;
    cout<<"Time cost: "<<(double)(time_map["READ"])/read_count<<endl;
    if (learned_index_used){
        cout<<"Read query through Learned index: "<<learned_index_count<<endl;
        cout<<"Time cost: "<<(double)learned_index_time/learned_index_count<<endl;
    }
    cout<<"rocksdb count: "<<rocksdb_count<<endl;
    cout<<"rocksdb time cost: "<<(double)rocksdb_time/rocksdb_count<<endl;
    cout<<"rocksdb time cost when fail: "<<(double)rocksdb_fail_time/learned_index_count<<endl;
    cout<<"rocksdb time cost when success: "<<(double)rocksdb_success_time/(read_count-learned_index_count)<<endl;
    cout<<"load kv time cost: "<<(double)load_kv_time/load_kv_count<<endl;
    assert(load_kv_count == read_count);
    cout<<"GC num: "<<gc_count<<endl;
    cout<<"GC write bytes: "<<gc_write_size<<endl;
    read_count = 0;
    write_count = 0;
    time_map.clear();
    learned_index_count=0;
    learned_index_time=0;
    rocksdb_count=0;
    rocksdb_time=0;
    rocksdb_fail_time=0;
    rocksdb_success_time=0;
    load_kv_time=0;
    load_kv_count=0;
    gc_count=0;
    gc_write_size=0;

}

int main(int argc, char **argv) {
    if (argc < 3) {
        // path of source reflects the workload data 
        fprintf(stderr, "%s <path of source> [num of loading kv pairs]\n", argv[0]);
        exit(-1);
    }
    auto flags = parse_flags(argc, argv);
    
    string keys_file_path = get_required(flags, "keys_file"); // required
    string index_type = get_with_default(flags, "index_type", "LearnedKV");
    unsigned long long num_kv = stoull(get_required(flags, "num_kv"));
    double over_provision_ratio = stod(get_with_default(flags, "over_provision_ratio", "0.3"));
    string distribution = get_with_default(flags, "distribution", "zipfian");

    // if(learned_index_used){
    //     cout<<"Learned index is used"<<endl;
    // }
    // else{
    //     cout<<"Learned index is not used"<<endl;
    // }
    
    //cout<<"GROUP_SIZE (MB): "<<1.0*GROUP_SIZE/(1024*1024)<<endl;
    if(index_type=="LearnedKV"){
        cout<<"Using LearnedKV"<<endl;
        learned_index_used = true;
    }
    else if (index_type=="RocksDB"){
        cout<<"Using RocksDB"<<endl;
        learned_index_used = false;
    }
    else{
        cout<<"Wrong index type"<<endl;
        exit(-1);
    }

    group_size = num_kv*(KEY_SIZE+VALUE_SIZE)*(1+over_provision_ratio);
    cout<<"GROUP_SIZE (MB): "<<static_cast<double>(group_size)/(1024*1024)<<endl;

    KVNUM = num_kv; // num of keys taken into operations

    // read the workload
    vector<pair<string,unsigned long long>> workloads; // [<op, key>, <op, eky>, ..., <op, key>]
    read_workload(workloads, keys_file_path);
    assert(KVNUM<=workloads.size());

    

    //Group* groups = new Group[numOfGroup];
    // LearnedKV *learnedkv = new LearnedKV;
    //RDB *learnedkv = new RDB;
    //BlobDB *learnedkv = new BlobDB;
    //HashKV *learnedkv = new HashKV;


    
    long read_count = 0;
    long write_count = 0;

    long scan_count = 0;

    if (!file_exists(output_path)) {
            std::ofstream ofile;
            ofile.open(output_path, std::ios::app);
            ofile << "id" << ",";
            ofile << "key_path" << ",";
            ofile << "index_type" << ",";
            ofile << "key size" << ",";
            ofile << "value size" << ",";
            ofile << "num of keys" << ",";
            ofile << "size limit (MB)" << ",";
            ofile << "P0 (load) num" << ",";
            ofile << "P0 throughput" << ",";
            ofile << "P1 (write) num" << ",";
            ofile << "P1 (read) num" << ",";
            ofile << "P1 (read through LI) num" << ",";
            ofile << "P1 write throughput" << ",";
            ofile << "P1 read throughput" << ",";
            ofile << "P2 (write) num" << ",";
            ofile << "P2 (read) num" << ",";
            ofile << "P2 (read through LI) num" << ",";
            ofile << "P2 write throughput" << ",";
            ofile << "P2 read throughput" << ",";
            ofile << "P3 (write) num" << ",";
            ofile << "P3 (read) num" << ",";
            ofile << "P3 (read through LI) num" << ",";
            ofile << "P3 write throughput" << ",";
            ofile << "P3 read throughput" << ",";
            ofile.close();
    }
    // time id
    std::time_t t = std::time(nullptr);
    char time_str[100];

    std::ofstream ofile;
    ofile.open(output_path, std::ios::app);
    ofile << endl;
    if (std::strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%S", std::localtime(&t))) {
        ofile << time_str << ',';
    }

    ofile << keys_file_path << ",";
    if (learned_index_used){
        ofile << "LearnedKV" << ",";
    }
    else{
        ofile << "RocksDB" << ",";
    }
    ofile << KEY_SIZE << ",";
    ofile << VALUE_SIZE << ",";
    ofile << KVNUM << ",";
    ofile << 1.0*group_size/(1024*1024) << ",";

    //ofile << std::endl;
    ofile.close();

    //bulk load
    char* index_name = "index_name";
    BTree bt(0, true, index_name, true);
    LeafNodeIterm *data = new LeafNodeIterm[KVNUM];
    KeyType *keys = new KeyType[KVNUM];
    for(size_t i = 0;i<KVNUM;i++){
        keys[i] = workloads[i].second;
    }
    sort(keys, keys+KVNUM);

    for(size_t i = 0;i<KVNUM;i++){
        unsigned long long key = workloads[i].second;
        //string value;
        //gen_value(key,value,VALUE_SIZE);
        data[i].key = keys[i];
        data[i].value = keys[i]+1;
    }
    bt.bulk_load(data, KVNUM);
    bt.sync_metanode();

    delete[] keys;

    // read/write
    int c = 0;
    long long sel;
    long long sml;
    long long inl;
    double total_smos = 0;
    double total_updated = 0;
    int update_c = 0;
    int smo_c = 0;
    for(size_t i = KVNUM;i<workloads.size();i++){
        if(i==KVNUM){
            cout<<"Finished loading initial keys"<<endl;
            
            cout<<"-----Throughput (KOPS)----------"<<endl;
            cout<<"Num of load operations: "<<KVNUM<<endl;
            cout<<"Throughput: "<<1000 * (double)KVNUM/time_map["INSERT"]<<endl;

            ofile.open(output_path, std::ios::app);
            ofile << KVNUM << ",";
            ofile << 1000 * (double)KVNUM/time_map["INSERT"] << ",";
            ofile.close();

            read_count = 0;
            write_count = 0;
        }
        else if(i%KVNUM == 0){
            print_throughtput(write_count, read_count);
        }
        string op = workloads[i].first;
        unsigned long long key = workloads[i].second;
        if(op=="UPDATE" || op=="INSERT"){
            
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);
            
            bt.insert_key_entry(key, key + 1, &sel, &sml, &inl, &smo_c, &update_c);
            
            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            write_count++;
        }
        else if(op=="READ"){
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);

            bool found = bt.lookup(key, &c);

            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            read_count++;
        }

    }

    print_throughtput(write_count, read_count);
    // print_time_breakdown(write_count, read_count);

    //delete learnedkv;

    return 0;
}
