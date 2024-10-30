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
//#include "group.h"
#include "./LearnedIndexDiskExp/code/ALEX/src/core/alex.h"
#include "alex.h"

using namespace std;

#define KeyType uint64_t
#define ValueType uint64_t

#define KEY_SIZE 8
#define VALUE_SIZE 1016

#define GROUP_SIZE 100 * 1000


unsigned int KVNUM;
unordered_map<string, long> time_map;

// long learned_index_time = 0;
// long rocksdb_time = 0;
// long rocksdb_success_time = 0;
// long rocksdb_fail_time = 0;
// long learned_index_count = 0;
// long rocksdb_count = 0;
// long load_kv_time = 0;
// long load_kv_count = 0;
// long gc_count = 0;
// long gc_write_size = 0;

string output_path = "./out.csv";


void gen_value(char* key, char* value, size_t size){
    memset(value,key[0],size);
}
bool file_exists(const std::string &str) {
    std::ifstream fs(str);
    return fs.is_open();
}

void read_workload(vector<pair<string,unsigned long long>>& workloads, char* file_path){ // pass by reference to avoid copying
    string line;
    ifstream myfile(file_path);
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

    std::ofstream ofile;
    ofile.open(output_path, std::ios::app);
    ofile << write_count << ",";
    ofile << read_count << ",";
    ofile << 0 << ",";
    ofile << 1000 * (double)(write_count)/time_map["UPDATE"] << ",";
    ofile << 1000 * (double)read_count/time_map["READ"] << ",";

    read_count = 0;
    write_count = 0;
    time_map.clear();
}


int main(int argc, char **argv) {
    if (argc < 3) {
        // path of source reflects the workload data 
        fprintf(stderr, "%s <path of source> [num of loading kv pairs]\n", argv[0]);
        exit(-1);
    }


    KVNUM = atoi(argv[2]); // num of keys taken into operations

    // read the workload
    vector<pair<string,unsigned long long>> workloads; // [<op, key>, <op, eky>, ..., <op, key>]
    read_workload(workloads, argv[1]);
    assert(KVNUM<=workloads.size());


    std::string index_name = "index_file";
    std::string data_file = index_name + "_data";
    Alex_competitor<KeyType, ValueType> index(0,const_cast<char *>(index_name.c_str()), const_cast<char *>(data_file.c_str()));

    
    long read_count = 0;
    long write_count = 0;

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

    ofile << argv[1] << ",";
    ofile << "ALEX" << ",";
    ofile << KEY_SIZE << ",";
    ofile << VALUE_SIZE << ",";
    ofile << KVNUM << ",";
    ofile << 1.0*GROUP_SIZE/(1024*1024) << ",";

    //ofile << std::endl;
    ofile.close();


    //bulk load
    

    auto values = new std::pair<KeyType, ValueType>[KVNUM];
    for (int i = 0; i < KVNUM; i++) {
        values[i].first = workloads[i].second;
        values[i].second = values[i].first;
    }
    struct timeval startTime, endTime;
    gettimeofday(&startTime, 0);

    index.bulk_load(values, KVNUM/2);

    gettimeofday(&endTime, 0);
    time_map["INSERT"] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);


    for (size_t i = KVNUM/2;i<workloads.size();i++){
        unsigned long long key = workloads[i].second;
        if(i==(size_t)KVNUM){
            cout<<"Finished loading initial keys"<<endl;
            
            // // comment this part if not needed
            // // dump all valid keys to learned index
            // for(size_t j = 0;j<numOfGroup;j++){
            //     groups[j].garbage_collection(KEY_SIZE,VALUE_SIZE);
            // }

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

            // // NOTE: only test for one batch of operations
            // break;
            //print_time_breakdown(write_count, read_count);
        }




        // determine read or write
        string op = workloads[i].first;
        cout<<"op: "<<op<<endl;
        if(op=="UPDATE" || op=="INSERT"){
            if(i>=KVNUM){continue;}

            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);

            //learnedkv->put(key_char, KEY_SIZE, value, VALUE_SIZE);
            //string value(VALUE_SIZE, 'x');
            ValueType value = key;
            cout<<"1: "<<op<<endl;
            index.put(key, value);
            cout<<"2: "<<op<<endl;

            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            write_count++;
        }
        else if(op=="READ"){
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);

            //learnedkv->get(key_char, KEY_SIZE, value, VALUE_SIZE);
            //index.get(key_char, KEY_SIZE, value, VALUE_SIZE);
            //string value;
            ValueType value;
            index.get(key,&value);

            //groups[groupId].get(key_char, KEY_SIZE, value, VALUE_SIZE);
            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            read_count++;

            
            // if(value!=std::string(1024, 'x')){
            //     cout<<"problematic key: "<<key<<endl;
            //     cout<<"index: "<<i<<endl;
            //     //cout<<"read value: "<<value<<endl;
            //     //cout<<"actual value: "<<new_value<<endl;
            // }
            // assert(value==std::string(1024, 'x'));
        }



    }

    print_throughtput(write_count, read_count);
    //print_time_breakdown(write_count, read_count);


    return 0;
}
