#ifndef BOURBON_WRAPPER_H
#define BOURBON_WRAPPER_H

#include <string>
#include "leveldb/db.h"
#include "util/coding.h"
#include "db/version_set.h"
#include "mod/learned_index.h"
// #include "utils.h"
#include <map>
#include <unordered_map>
#include "leveldb/write_batch.h"
#include "leveldb/options.h"
#include <bitset>
#include <vector>
#include <functional>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include <arpa/inet.h>

using namespace std;

#define KEY_SIZE 8 // 8 bytes

#define VALUE_SIZE 1016 // KV size is 1024 bytes


// NOTE: using GROUP_SIZE to represent the condition of GC
//#define GROUP_SIZE (unsigned long long)(1.3*10*1000*1000) * 1024ULL // 1.3 * 100K * 1KB = 1300MB

#define ERR_BOUND 1000

#define KEY_TYPE uint64_t
#define offset_t uint64_t

#define VLOG_PATH "db/vlog"
#define DB_PATH "db"

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

using namespace std;


class BourbonWrapper {
public:
    leveldb::DB* db;
    leveldb::Options options;
    leveldb::ReadOptions read_options;
    leveldb::WriteOptions write_options;

    unsigned long long write_pos;

    BourbonWrapper();
    ~BourbonWrapper();

    void initialize(const std::string& db_location);
    void destroy();
    std::string get(string key, uint64_t keySize);
    void put(string key, uint64_t keySize, const std::string& value, uint64_t valueSize);
    void garbage_collection(uint64_t keySize, uint64_t valueSize);
    void shutdown();

    std::string generate_key(const std::string& key);
};

inline std::map<std::string, std::string> parse_flags2(int argc, char** argv) {
  std::map<std::string, std::string> flags;
  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    size_t equals = arg.find("=");
    size_t dash = arg.find("--");
    if (dash != 0) {
      std::cout << "Bad flag '" << argv[i] << "'. Expected --key=value"
                << std::endl;
      continue;
    }
    std::string key = arg.substr(2, equals - 2);
    std::string val;
    if (equals == std::string::npos) {
      val = "";
    } else {
      val = arg.substr(equals + 1);
    }
    flags[key] = val;
  }
  return flags;
}

inline std::string get_with_default2(const std::map<std::string, std::string>& m,
                             const std::string& key,
                             const std::string& defval) {
  auto it = m.find(key);
  if (it == m.end()) {
    std::cout << key << " = " << defval << std::endl;
    return defval;
  }
  std::cout << key << " = " << it->second << std::endl;
  return it->second;
}

inline std::string get_required2(const std::map<std::string, std::string>& m,
                         const std::string& key) {
  auto it = m.find(key);
  if (it == m.end()) {
    std::cout << "Required flag --" << key << " was not found" << std::endl;
  }
  std::cout << key << " = " << it->second << std::endl;
  return it->second;
}


// Check for endianness and convert accordingly
inline unsigned long long htonll2(unsigned long long value) {
    if (htonl(1) != 1) {
        uint32_t high_part = htonl(static_cast<uint32_t>(value >> 32));
        uint32_t low_part = htonl(static_cast<uint32_t>(value & 0xFFFFFFFFLL));
        return (static_cast<unsigned long long>(low_part) << 32) | high_part;
    } else {
        return value;
    }
}

inline  long long ntohll2(unsigned long long value) {
    if (htonl(1) != 1) {
        uint32_t high_part = ntohl(static_cast<uint32_t>(value >> 32));
        uint32_t low_part = ntohl(static_cast<uint32_t>(value & 0xFFFFFFFFLL));
        return (static_cast<unsigned long long>(low_part) << 32) | high_part;
    } else {
        return value;
    }
}


// Convert 8B string to unsigned long long
inline unsigned long long stringToULL2(const std::string& str) {
    unsigned long long ull = 0;
    memcpy(&ull, str.data(), sizeof(unsigned long long));
    return ntohll2(ull);;
}

// Convert unsigned long long to 8B string
inline std::string ullToString2(unsigned long long ull) {
    ull = htonll2(ull);
    std::string str(sizeof(unsigned long long), '\0');
    memcpy(&str[0], &ull, sizeof(unsigned long long));
    return str;
}


#endif // BOURBON_WRAPPER_H