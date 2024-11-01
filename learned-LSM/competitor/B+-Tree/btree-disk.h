#ifndef INDEXES_BTREE_DISK_H_
#define INDEXES_BTREE_DISK_H_

#include "./UpdatableLearnedIndexDisk/B+Tree/b_tree.h"
// #include "../base_index.h"
#include <string>
#include <vector>
#include <iostream>
#include <cstdint> 

using namespace std;

string main_file = "./btree_disk";
typedef uint64_t K;
typedef uint64_t V;

class BaselineBTreeDisk {
 public:

  BaselineBTreeDisk()
      : index_file_(main_file),
        btree_(0, true, const_cast<char*>(index_file_.c_str()), true),
        cnt_(0),
        inner_on_disk_num_(2) {} // possibly 0,1,2

  void Build(vector<pair<K, V>>& key_value) {
    vector<pair<K, V>> dataset(key_value.begin(),key_value.end());
    LeafNodeIterm* data = new LeafNodeIterm[key_value.size()];
    for (int i = 0; i < key_value.size(); i++) {
      data[i].key = key_value[i].first;
      data[i].value = key_value[i].second;
    }
    btree_.bulk_load(data, key_value.size(), 0.7, inner_on_disk_num_);
    btree_.sync_metanode();

  }

  V Find(const K key) {
    int c = 0;
    V res = 0;
    bool found = btree_.lookup(key, &c, &res);
#ifdef CHECK_CORRECTION
    if (!found) {
      std::cout << "btree baseline find key " << key << " wrong! res:" << res
                << std::endl;
      btree_.lookup(key, &c, &res);
    }
#endif
    cnt_ += c;
    return res;
  }


  bool Insert(const K key, const V value) {
    btree_.insert_key_entry(key, value);
    btree_.sync_metanode();
    return true;
  }

  bool Update(const K key, const V value) {
    int c = 0;
    bool res = btree_.update(key, value, &c);
    cnt_ += c;
    return res;
  }



 private:
  std::string name_ = "BASELINE_BTree_Disk";
  std::string index_file_;
  BTree btree_;
  int inner_on_disk_num_;

  int cnt_;
};

#endif  // !INDEXES_BTREE_DISK_H_