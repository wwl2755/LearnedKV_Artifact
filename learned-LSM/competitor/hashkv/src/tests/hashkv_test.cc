#include "util/debug.hh"
#include "define.hh"
#include "kvServer.hh"
#include <ctime>
#include <chrono>
#include <iostream>
#include <stdlib.h>     // srand(), rand()
#include "../statsRecorder.hh"

using namespace std;

#define VALUE_SIZE  (1016)

#define VAR_SIZE (1)
#define KV_NUM_DEFAULT      (100 * 1000)
#define DISK_SIZE           (1024ULL * 1024 * 1024 * 2) // 1GB
#define UPDATE_RUNS (5)

#define GEN_VALUE(v, i, size)  \
    do { memset(v, 2+i, size); } while(0);

#define SKIPTHIS (ki % (ConfigManager::getInstance().getMainSegmentSize() / VALUE_SIZE) % 10 > 2)

#define DELTHIS (i % 11 < 2)

static int runCount = 0;
int KVNUM = KV_NUM_DEFAULT;
bool randkey = true;
int exitCode = 0;
unordered_map<string, long> time_map;

string output_path = "../../out.csv";

std::unordered_map<int, std::string> loadKeys;

void gen_value(char* key, char* value, size_t size){
    memset(value,key[0],size);
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
    cout<<"Throughput: "<<1000 * (double)read_count/time_map["READ"]<<endl;

    cout<<"Total throughput: "<<1000 * (double)(write_count+read_count)/(time_map["UPDATE"]+time_map["READ"])<<endl;

    std::ofstream ofile;
    ofile.open(output_path, std::ios::app);
    ofile << 1000 * (double)(write_count+read_count)/(time_map["UPDATE"]+time_map["READ"]) << ",";


    read_count = 0;
    write_count = 0;
    time_map.clear();
}

void reportUsage(KvServer &kvServer) {
    kvServer.printStorageUsage();
    kvServer.printBufferUsage();
    kvServer.printKeyCacheUsage();
    //kvServer.printGroups();
}

void timer (bool start = true, const char *label = 0) {
    static std::chrono::system_clock::time_point startTime;
    if (start) {
        startTime = std::chrono::system_clock::now();
    } else {
        std::chrono::system_clock::time_point endTime = std::chrono::system_clock::now();
        printf("Elapsed Time (%s): %.6lf s\n", label, std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000.0 / 1000.0);
    }
}

void startTimer() {
    timer(true);
}

void stopTimer(const char *label) {
    timer(false, label);
}

int main(int argc, char **argv) {
    runCount = 0;

    if (argc < 2) {
        fprintf(stderr, "%s <data directory> [num of KV pairs]\n", argv[0]);
        exit(-1);
    }

    // how many keys to generate
    if (argc >= 3) {
        KVNUM = atoi(argv[2]);
    }

    fprintf(stderr,"> Beginning of tests (KVNUM=%d)\n", KVNUM);
    ConfigManager::getInstance().setConfigPath("config.ini");

    // data disks
    DiskInfo disk1(0, argv[1], DISK_SIZE);

    std::vector<DiskInfo> disks;
    disks.push_back(disk1);

    DeviceManager diskManager(disks);

    KvServer kvserver(&diskManager);



    // read the workload
    // string keys_file_path = "output_1M_YCSB_c.txt";
    string keys_file_path = argv[3];
    vector<pair<string,unsigned long long>> workloads; // [<op, key>, <op, eky>, ..., <op, key>]
    read_workload(workloads, keys_file_path);

    long read_count = 0;
    long write_count = 0;

    if (!file_exists(output_path)){
        std::ofstream ofile;
        ofile.open(output_path, std::ios::app);
        ofile << "id" << ",";
        ofile << "key_path" << ",";
        ofile << "index_type" << ",";
        ofile << "total throughput" << ",";
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
    ofile << "HashKV" << ",";
    ofile.close();

    for (size_t i = 0;i<workloads.size();i++){
        unsigned long long key = workloads[i].second;
        if(i==(size_t)KVNUM){
            cout<<"Finished loading initial keys"<<endl;
            


            cout<<"-----Throughput (KOPS)----------"<<endl;
            cout<<"Num of load operations: "<<KVNUM<<endl;
            cout<<"Throughput: "<<1000 * (double)KVNUM/time_map["INSERT"]<<endl;



            read_count = 0;
            write_count = 0;
            time_map.clear();
        }
        else if(i%KVNUM == 0 && i!=0){
            print_throughtput(write_count, read_count);

        }


        // determine read or write
        string op = workloads[i].first;
        //cout<<"op: "<<op<<endl;
        if(op=="UPDATE" || op=="INSERT"){
            // insert each key to its group
            char key_char[KEY_SIZE]; // excluding the null terminator
            char value[VALUE_SIZE];
            memset(key_char, 0, sizeof(key_char));
            memset(value, 0, sizeof(value));
            
            memcpy(key_char, &key, sizeof(unsigned long long));
            
            gen_value(key_char,value,VALUE_SIZE);
            
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);
            
            kvserver.putValue(key_char, KEY_SIZE, value, VALUE_SIZE);

            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            write_count++;
        }
        else if(op=="READ"){
            char key_char[KEY_SIZE]; // excluding the null terminator
            //char value[VALUE_SIZE+1];
            char * value;
            memset(key_char, 0, sizeof(key_char));
            //memset(value, 0, sizeof(value));

            //ull_to_char(key_char, key, KEY_SIZE);
            //char key_data[sizeof(unsigned long long)];
            memcpy(key_char, &key, sizeof(unsigned long long));

            //gen_value(key_char,value,VALUE_SIZE);
            //groups[groupId].put(key_char, KEY_SIZE, value, VALUE_SIZE);
            struct timeval startTime, endTime;
            gettimeofday(&startTime, 0);
            len_t myValueSize = VALUE_SIZE;
            kvserver.getValue(key_char, KEY_SIZE, value, myValueSize);

            //kvserver.getValue(key_char, KEY_SIZE, value, VALUE_SIZE);
            //learnedkv->get(key_char, KEY_SIZE, value, VALUE_SIZE);
            //groups[groupId].get(key_char, KEY_SIZE, value, VALUE_SIZE);
            
            gettimeofday(&endTime, 0);
            time_map[op] += (endTime.tv_sec - startTime.tv_sec) * 1000000 + (endTime.tv_usec - startTime.tv_usec);
            read_count++;

            // char new_value[VALUE_SIZE];
            // memset(new_value, 0, sizeof(new_value));
            // gen_value(key_char,new_value,VALUE_SIZE);
            // if(strcmp(value,new_value)!=0){
            //     cout<<"problematic key: "<<key<<endl;
            //     cout<<"index: "<<i<<endl;
            //     cout<<"read value: "<<value<<endl;
            //     cout<<"actual value: "<<new_value<<endl;
            //     break;
            // }
            // assert(strcmp(value,new_value)==0);

            delete value;
        }



    }
    print_throughtput(write_count, read_count);

    fprintf(stderr,"> End of update tests\n");

    return exitCode;
}
