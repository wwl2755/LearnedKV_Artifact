#include <iostream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include "../utils.h"
#include <arpa/inet.h>
// #include "./b+_tree/b_tree.h"
#include "btree-disk.h"

using namespace std;

int main(int argc, char **argv) {

    BaselineBTreeDisk bt;
    vector<pair<uint64_t, uint64_t>> key_value;
    for (int i = 0; i < 100; i++) {
        key_value.push_back(make_pair(i+100, i + 1));
    }
    bt.Build(key_value);
    cout << bt.Find(1) << endl;
    cout << bt.Find(101) << endl;

    return 0;
}
