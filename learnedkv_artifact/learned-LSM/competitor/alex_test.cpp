#include <iostream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <algorithm>
#include <iomanip>
#include <random>
#include <climits>
#include "../utils.h"
#include <arpa/inet.h>
// #include "./b+_tree/b_tree.h"
// #include "btree-disk.h"
#include "alex-disk.h"

using namespace std;

int main(int argc, char **argv) {

    // Create a random device to seed the random number generator
    std::random_device rd;

    // Initialize a Mersenne Twister 64-bit engine
    std::mt19937_64 gen(rd());

    // Define the range for the random number
    std::uniform_int_distribution<unsigned long long> dis(0, ULLONG_MAX);

    BaselineAlexDisk bt;
    vector<pair<unsigned long long , unsigned long long>> key_value;
    for (int i = 100; i < 1000; i++) {
        //generate a random key

        unsigned long long random_number = dis(gen);



        key_value.push_back(make_pair(random_number, random_number + 1));
    }
    // for (int i = 1000; i < 2000; i++) {
    //     key_value.push_back(make_pair(2*i, 2*i + 1));
    // }
    sort(key_value.begin(), key_value.end());

    bt.Build(key_value);
    cout << "key: "<< key_value[0].first << endl;
    cout << "Value: "<< bt.Find(key_value[0].first) << endl;
    cout << "Value: "<< key_value[0].second << endl;
    // cout << bt.Find(201) << endl;

    return 0;
}
