#ifndef INDEXES_ALEX_DISK_H_
#define INDEXES_ALEX_DISK_H_

#include "./UpdatableLearnedIndexDisk/ALEX/alex.h"
// #include "../base_index.h"
#include <string>
#include <vector>
#include <iostream>
#include <cstdint> 

using namespace std;

string main_file = "./alex_disk";
typedef unsigned long long  K;
typedef unsigned long long  V;

class BaselineAlexDisk  {
 public:
  struct param_t {
    std::string main_file;
  };

  BaselineAlexDisk()
      : index_file_(main_file),
        alex_(0, true, const_cast<char*>(index_file_.c_str()),
              const_cast<char*>((index_file_ + "_data").c_str())),
        block_cnt_(0),
        level_cnt_(0),
        search_time(0),
        insert_time(0),
        smo_time(0),
        maintain_time(0),
        smo_count(0) {}

  void Build(vector<pair<K, V>>& key_value) {
    // alex_.bulk_load(data.data(), data.size());
    alex_.bulk_load(key_value.data(), key_value.size());
    alex_.sync_metanode(true);
    alex_.sync_metanode(false);
    std::cout << "\nALEX use " << alex_.stats_.num_model_nodes << " models for "
              << key_value.size() << " records" << std::endl;
  }

  V Find(const K key) {
    int c = 0, l = 0, inner = 0;
    V res = 0;
    alex_.get_leaf_disk(key, &res, &c, &l, &inner);
    block_cnt_ += c;
    level_cnt_ += l;
    return res;
  }



  bool Insert(const K key, const V value) {
    long long sel = 0, sml = 0, inl = 0, mal = 0;
    int smo = 0;

    alex_.insert_disk(key, value, &sel, &inl, &sml, &mal, &smo);
    alex_.sync_metanode(true);
    alex_.sync_metanode(false);

    search_time += sel;
    insert_time += sml;
    smo_time += inl;
    maintain_time += mal;
    smo_count += smo;
    return true;
  }


 private:
  std::string name_ = "BASELINE_ALEX_DISK";
  std::string index_file_;
  alex_disk::Alex<K, V> alex_;

  int block_cnt_;
  int level_cnt_;
  long long search_time;
  long long insert_time;
  long long smo_time;
  long long maintain_time;
  int smo_count;
};

#endif  // !INDEXES_ALEX_DISK_H_