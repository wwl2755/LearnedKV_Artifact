#pragma once

#include <bitset>
#include <vector>
#include <functional>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <arpa/inet.h>

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class BloomFilter {
public:
    // Constructor: specify the size of the bitset and the number of hash functions
    BloomFilter(size_t size, size_t numHashes) 
        : bitset(size), numHashes(numHashes) {}

    // Insert a key into the Bloom filter
    void insert(unsigned long long key) {
        for (size_t i = 0; i < numHashes; i++) {
            bitset[hash(key, i) % bitset.size()] = 1;
        }
    }

    // Check if a key is possibly in the set
    bool possiblyContains(unsigned long long key) const {
        for (size_t i = 0; i < numHashes; i++) {
            if (!bitset[hash(key, i) % bitset.size()]) {
                return false; // Key is definitely not in the set
            }
        }
        return true; // Key possibly is in the set
    }

    // Re-initialize (or reset) the Bloom filter
    void reset() {
        bitset.reset();
    }

private:
    // Simple hash function (you might want more complex hash functions for real-world scenarios)
    size_t hash(unsigned long long key, size_t i) const {
        return std::hash<unsigned long long>()(key + i);
    }

    std::bitset<1000000> bitset; // Adjust size accordingly
    size_t numHashes;
};

// int main() {
//     BloomFilter filter(1000000, 5); // 1,000,000 bits, 5 hash functions

//     unsigned long long key = 12345678901234567ULL;
//     filter.insert(key);

//     if (filter.possiblyContains(key)) {
//         std::cout << "Key possibly exists in the set." << std::endl;
//     } else {
//         std::cout << "Key definitely does not exist in the set." << std::endl;
//     }
// }

// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

std::map<std::string, std::string> parse_flags(int argc, char** argv) {
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

std::string get_with_default(const std::map<std::string, std::string>& m,
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

std::string get_required(const std::map<std::string, std::string>& m,
                         const std::string& key) {
  auto it = m.find(key);
  if (it == m.end()) {
    std::cout << "Required flag --" << key << " was not found" << std::endl;
  }
  std::cout << key << " = " << it->second << std::endl;
  return it->second;
}

bool get_boolean_flag(const std::map<std::string, std::string>& m,
                      const std::string& key) {
  auto res = m.find(key) != m.end();
  std::cout << key << " = " << res << std::endl;
  return res;
}

std::vector<std::string> get_comma_separated(
    std::map<std::string, std::string>& m, const std::string& key) {
  std::vector<std::string> vals;
  auto it = m.find(key);
  if (it == m.end()) {
    return vals;
  }
  std::istringstream s(m[key]);
  std::string val;
  std::cout << key << " = " << it->second << std::endl;
  while (std::getline(s, val, ',')) {
    vals.push_back(val);
  }
  return vals;
}

// Check for endianness and convert accordingly
unsigned long long htonll(unsigned long long value) {
    if (htonl(1) != 1) {
        uint32_t high_part = htonl(static_cast<uint32_t>(value >> 32));
        uint32_t low_part = htonl(static_cast<uint32_t>(value & 0xFFFFFFFFLL));
        return (static_cast<unsigned long long>(low_part) << 32) | high_part;
    } else {
        return value;
    }
}

unsigned long long ntohll(unsigned long long value) {
    if (htonl(1) != 1) {
        uint32_t high_part = ntohl(static_cast<uint32_t>(value >> 32));
        uint32_t low_part = ntohl(static_cast<uint32_t>(value & 0xFFFFFFFFLL));
        return (static_cast<unsigned long long>(low_part) << 32) | high_part;
    } else {
        return value;
    }
}


// Convert 8B string to unsigned long long
unsigned long long stringToULL(const std::string& str) {
    unsigned long long ull = 0;
    memcpy(&ull, str.data(), sizeof(unsigned long long));
    return ntohll(ull);;
}

// Convert unsigned long long to 8B string
std::string ullToString(unsigned long long ull) {
    ull = htonll(ull);
    std::string str(sizeof(unsigned long long), '\0');
    memcpy(&str[0], &ull, sizeof(unsigned long long));
    return str;
}
