#include "BourbonWrapper.h"
#include <iostream>
// #include <rocksdb/db.h>
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
// #include "../../utils.h"

using namespace std;

// unsigned long long scanRange = 10000000000000000ULL; // 1*10^16

// group config
// int numOfGroup = 1;
// bool hash_based = false; // True: KV pairs are separated by hash values; False: Separated by key range

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

string output_path = "../out.csv";

unsigned long long group_size = 0;

void gen_value(string key, std::string& value, size_t size) {
    // Convert the ULL to a character (e.g., use the lowest byte)
    // char key_char = static_cast<char>(key & 0xFF);
    char key_char = key[0];

    // Resize the string to the desired size
    value.resize(size);

    // Fill the string with the character derived from key
    memset(&value[0], key_char, size);
}
bool file_exists(const std::string &str) {
    std::ifstream fs(str);
    return fs.is_open();
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
    cout<<"Throughput: "<<1000 * (double)(read_count)/time_map["READ"]<<endl;
    if (learned_index_used){
        cout<<"Read query through Learned index: "<<learned_index_count<<endl;
        cout<<"Throughput: "<<1000* (double)learned_index_count/learned_index_time<<endl;
    }

    cout<<"Total throughput: "<<1000 * (double)(write_count+read_count)/(time_map["UPDATE"]+time_map["READ"])<<endl;

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
    ofile << 1000 * (double)(read_count)/time_map["READ"] << ",";

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

int main(int argc, char **argv) {
    // BourbonWrapper bourbon;
    // std::string db_location = "./db";

    // // Initialize
    // bourbon.initialize(db_location);

    // // Put operation
    // bourbon.put(1234, sizeof(unsigned long long), "test_value", 10);

    // // Get operation
    // std::string retrieved_value = bourbon.get(1234, sizeof(unsigned long long));
    // std::cout << "Retrieved value: " << retrieved_value << std::endl;


    // Shutdown is called automatically in the destructor


    if (argc < 3) {
        // path of source reflects the workload data 
        fprintf(stderr, "%s <path of source> [num of loading kv pairs]\n", argv[0]);
        exit(-1);
    }
    auto flags = parse_flags2(argc, argv);
    
    string keys_file_path = get_required2(flags, "keys_file"); // required
    // string index_type = get_with_default2(flags, "index_type", "LearnedKV");
    unsigned long long num_kv = stoull(get_required2(flags, "num_kv"));
    double over_provision_ratio = stod(get_with_default2(flags, "over_provision_ratio", "0.3"));
    string distribution = get_with_default2(flags, "distribution", "zipfian");


    group_size = num_kv*(KEY_SIZE+VALUE_SIZE)*(1+over_provision_ratio);
    cout<<"GROUP_SIZE (MB): "<<static_cast<double>(group_size)/(1024*1024)<<endl;

    KVNUM = num_kv; // num of keys taken into operations

    // read the workload
    vector<pair<string,unsigned long long>> workloads; // [<op, key>, <op, eky>, ..., <op, key>]
    read_workload(workloads, keys_file_path);
    assert(KVNUM<=workloads.size());

    

    // LearnedKV *learnedkv = new LearnedKV;
    BourbonWrapper bourbon;
    // std::string db_location = "./db";

    // Initialize
    // bourbon.initialize(DB_PATH);


    
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


    //builk load
    // learnedkv->bulk_load(workloads,KVNUM);
    // cout<<"Bulk load finished"<<endl;


    for (size_t i = 0;i<workloads.size();i++){
        // unsigned long long key = workloads[i].second;
        string key = to_string(workloads[i].second);
        key.resize(KEY_SIZE);
        if(i==(size_t)KVNUM){
            cout<<"Finished loading initial keys"<<endl;
            
            // comment this part if not needed
            // dump all valid keys to learned index
            // learnedkv->garbage_collection(KEY_SIZE,VALUE_SIZE);
            // bourbon.garbage_collection(KEY_SIZE,VALUE_SIZE);

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
        else if(i%KVNUM == 0 && i!=0){
            print_throughtput(write_count, read_count);

        }

        // locate the group
        int groupId = 0;


        // determine read or write
        string op = workloads[i].first;
        //cout<<"op: "<<op<<endl;
        if(op=="UPDATE" || op=="INSERT"){
            // cout<<"UPDATE key: "<<key<<endl;


            string value;
            
            gen_value(key,value,VALUE_SIZE);

            
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);
            // learnedkv->put(key, KEY_SIZE, value, VALUE_SIZE);

            // Put operation
            bourbon.put(key, KEY_SIZE, value, VALUE_SIZE);

            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            write_count++;
        }
        else if(op=="READ"){
            // cout<<"READ key: "<<key<<endl;

            string value;

            //gen_value(key_char,value,VALUE_SIZE);
            //groups[groupId].put(key_char, KEY_SIZE, value, VALUE_SIZE);
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);
            // int res = learnedkv->get(key, KEY_SIZE, value, VALUE_SIZE);
            //groups[groupId].get(key_char, KEY_SIZE, value, VALUE_SIZE);

            // Get operation
            value = bourbon.get(key, KEY_SIZE);
            // std::cout << "Retrieved value: " << retrieved_value << std::endl;

            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            read_count++;

        }


    }


    
    print_throughtput(write_count, read_count);
    // print_time_breakdown(write_count, read_count);

    // delete learnedkv;

    return 0;
}