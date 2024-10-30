#include <iostream>
#include <leveldb/db.h>
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
#include <arpa/inet.h>
#include <iomanip>
//#include <Eigen/Dense>
//#include <leveldb/options.h>
// #include "leveldb/include/leveldb/db.h"
// #include "leveldb/include/leveldb/options.h"
//#include "group.h"

using namespace std;
inline uint64_t htonll(uint64_t x) {
    #if BYTE_ORDER == LITTLE_ENDIAN
    uint32_t hi = htonl(x >> 32);
    uint32_t lo = htonl(x & 0xFFFFFFFFLL);
    return ((uint64_t)lo << 32) | hi;
    #else
    return x;
    #endif
}

inline uint64_t ntohll(uint64_t x) {
    #if BYTE_ORDER == LITTLE_ENDIAN
    uint32_t hi = ntohl(x >> 32);
    uint32_t lo = ntohl(x & 0xFFFFFFFFLL);
    return ((uint64_t)lo << 32) | hi;
    #else
    return x;
    #endif
}

// group config
int numOfGroup = 10;
bool hash_based = false; // True: KV pairs are separated by hash values; False: Separated by key range

bool learned_index_used = false;

unsigned int KVNUM;
unordered_map<string, long> time_map;

long learned_index_time = 0;
long leveldb_time = 0;
long learned_index_count = 0;
long leveldb_count = 0;
long gc_count = 0;
long gc_write_size = 0;

#define KEY_SIZE 8 // 8 bytes
#define VALUE_SIZE 6 // KV size is 1024 bytes


void gen_value(char* key, char* value, size_t size){
    memset(value,key[0],size);
}
// void ull_to_char(char* key, unsigned long long ull, size_t size){
//     for(int j=size-1;j>=0;j--){
//         int tmp = ull%256;
//         ull = ull/256;
//         key[j] = static_cast<char>(tmp);
//     }
// }




int main(int argc, char **argv) {

    leveldb::DB* db;
    leveldb::Options options;
    leveldb::Status status = leveldb::DestroyDB("./tmp", options);
    options.create_if_missing = true;
    status = leveldb::DB::Open(options, "./tmp", &db);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString() << std::endl;
      return -1;
    }

    unsigned long long key = 8;
    cout<<"key: ";
    for (std::size_t i = 0; i < sizeof(key); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(&key)[i]) << " ";
    }
    cout<<endl;

    char key_char[KEY_SIZE+1]; // excluding the null terminator
    char value[VALUE_SIZE+1];
    memset(key_char, 0, sizeof(key_char));
    memset(value, 0, sizeof(value));

    memcpy(key_char, &key, sizeof(unsigned long long));
    cout<<"key char: ";
    for (std::size_t i = 0; i < sizeof(key_char)-1; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(key_char)[i]) << " ";
    }
    cout<<endl;

    gen_value(key_char,value,VALUE_SIZE);
    cout<<"value: ";
    for (std::size_t i = 0; i < sizeof(value)-1; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(value)[i]) << " ";
    }
    cout<<endl;

    // put
    // NOTE: write_pos should be smaller than 10^8
    // TODO: can use unsigned int to present
    
    unsigned long long write_pos = 100000;
    //unsigned long long write_pos = 256*256 +4;
    cout<<dec<<"write_pos: ";
    cout<<write_pos<<endl;
    for (std::size_t i = 0; i < sizeof(write_pos); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(&write_pos)[i]) << " ";
    }
    cout<<endl;

    char ptr[sizeof(unsigned long long)+1];
    //unsigned char * ptr = new unsigned char [sizeof(uint64_t)+1];
    memset(ptr,0,sizeof(ptr));
    //memcpy(ptr, &write_pos, sizeof(unsigned long long));
    //ptr = to_string(write_pos).c_str();
    memcpy(ptr,to_string(write_pos).c_str(),8);
    //ptr[8]='6';
    cout<<"ptr: "<<ptr<<endl;
    for (std::size_t i = 0; i < sizeof(ptr)-1; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(ptr)[i]) << " ";
    }
    cout<<endl;

    status = db->Put(leveldb::WriteOptions(), key_char, ptr);
    if (!status.ok()) {
      std::cerr << "Failed to write to database: " << status.ToString() << std::endl;
      return -1;
    }
    // for (size_t i = 0; i < sizeof(key_char); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(key_char[i]) << ' ';
    // }
    // cout<<endl;
    // for (size_t i = 0; i < sizeof(ptr); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ptr[i]) << ' ';
    // }
    // cout<<endl;

    // get
    string result;
    cout<<"key_char: ";
    for (std::size_t i = 0; i < sizeof(key_char)-1; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(key_char)[i]) << " ";
    }
    cout<<endl;
    status = db->Get(leveldb::ReadOptions(), key_char, &result);
    if (!status.ok()) {
      std::cerr << "Failed to read from database: " << status.ToString() << std::endl;
      return -1;
    }
    //std::cout << "stoi: " << stoull(result) << '\n';
    cout<<"result: ";
    for (std::size_t i = 0; i < sizeof(result); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(&result)[i]) << " ";
    }
    cout<<endl;

    cout<<"result.c_str(): ";
    for (std::size_t i = 0; i < sizeof(result.c_str()); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<const std::uint8_t*>(result.c_str())[i]) << " ";
    }
    cout<<endl;


    unsigned long long pos = 0;
    char result_char[sizeof(unsigned long long)+1];
    memset(result_char,0,sizeof(result_char));
    //strncpy(result_char,result.c_str(),sizeof(uint64_t));
    //memcpy(result_char,result.c_str(),sizeof(unsigned long long));
    strncpy(result_char,result.c_str(),8);
    cout<<"result_char: ";
    for (std::size_t i = 0; i < sizeof(result_char)-1; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(result_char)[i]) << " ";
    }
    cout<<endl;

    //memcpy(&pos,result_char,sizeof(unsigned long long));
    pos = stoull(result_char);
    cout<<"pos: ";
    cout<<dec<<pos<<endl;
    for (std::size_t i = 0; i < sizeof(pos); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(&pos)[i]) << " ";
    }
    cout<<endl;
    //cout<<"size of result string: "<<sizeof(result_char)<<endl; // 20

    // for (size_t i = 0; i < sizeof(result_char); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result_char[i]) << ' ';
    // }
    // cout<<endl;
    // for (size_t i = 0; i < sizeof(result.c_str()); ++i) {
    //     std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result.c_str()[i]) << ' ';
    // }
    // cout<<endl;

    //cout<<"result: "<<result<<endl;

    //cout<<hex<<pos<<endl;
    if(strcmp(ptr,result.c_str())==0 && write_pos==pos){
        cout<<"correct"<<endl;
        cout<<hex<<ptr<<endl;
        cout<<hex<<result.c_str()<<endl;
    }

    delete db;

    return 0;
}


/*
int main(int argc, char **argv){
    leveldb::DB* db;
    leveldb::Options options;
    options.create_if_missing = true;

    // Open the database
    leveldb::Status status = leveldb::DB::Open(options, "./path", &db);
    if (!status.ok()) {
        std::cerr << "Failed to open database: " << status.ToString() << std::endl;
        return 1;
    }

    unsigned long long key = 255;
    for (std::size_t i = 0; i < sizeof(key); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(&key)[i]) << " ";
    }
    cout<<endl;
    char key_str[9];
    //sprintf(key_str, "%08llx", key);
    memcpy(key_str,&key,sizeof(key)-1);
    for (size_t i = 0; i < sizeof(key_str); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(key_str)[i]) << ' ';
    }
    cout<<endl;

    unsigned long long write_pos = 12343321;
    for (std::size_t i = 0; i < sizeof(write_pos); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(&write_pos)[i]) << " ";
    }
    cout<<endl;
    char value_char[9];
    //sprintf(value_char, "%08llx", write_pos);
    memcpy(value_char,&write_pos,sizeof(write_pos));
    for (std::size_t i = 0; i < sizeof(value_char)-1; ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(value_char)[i]) << " ";
    }
    cout<<endl;


    // std::string value = "some value";
    status = db->Put(leveldb::WriteOptions(), key_str, value_char);
    if (!status.ok()) {
        std::cerr << "Failed to insert key-value pair: " << status.ToString() << std::endl;
    }

    std::string value_str;
    status = db->Get(leveldb::ReadOptions(), key_str, &value_str);
    if (status.ok()) {
        //unsigned long long value = std::strtoull(value_str.c_str(), nullptr, 0);
        char tmp [9];
        strncpy(tmp,value_str.c_str(),8);
        for (std::size_t i = 0; i < sizeof(tmp); ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(reinterpret_cast<std::uint8_t*>(tmp)[i]) << " ";
        }
        cout<<endl;
        unsigned long long value =0;
        memcpy(&value,tmp,8);
        std::cout << "Value for key " << key << " is: " << value << std::endl;
    } else {
        std::cerr << "Failed to read value for key " << key << ": " << status.ToString() << std::endl;
    }

    // Close the database
    delete db;
}*/

