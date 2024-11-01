#include <cassert>
#include <chrono>
#include <iostream>
#include "leveldb/db.h"
#include "leveldb/comparator.h"
#include "util.h"
#include "stats.h"
#include "learned_index.h"
#include <cstring>
#include "cxxopts.hpp"
#include <unistd.h>
#include <fstream>
#include "../db/version_set.h"
#include <cmath>
#include <random>

using namespace leveldb;
using namespace adgMod;
using std::string;
using std::cout;
using std::endl;
using std::to_string;
using std::vector;
using std::map;
using std::ifstream;
using std::string;

int num_pairs_base = 1000;
int mix_base = 20;

int main(int argc, char *argv[]) {
    int num_operations, num_iteration, num_mix;
    float test_num_segments_base;
    float num_pair_step;
    string db_location, profiler_out, input_filename, distribution_filename, ycsb_filename;
    bool print_single_timing, print_file_info, evict, unlimit_fd, use_distribution = false, pause, use_ycsb = false;
    bool change_level_load, change_file_load, change_level_learning, change_file_learning;
    int load_type, insert_bound, length_range;
    string db_location_copy;

    cxxopts::Options commandline_options("leveldb read test", "Testing leveldb read performance.");
    commandline_options.add_options()
            ("n,get_number", "the number of gets (to be multiplied by 1024)", cxxopts::value<int>(num_operations)->default_value("1000"))
            ("s,step", "the step of the loop of the size of db", cxxopts::value<float>(num_pair_step)->default_value("1"))
            ("i,iteration", "the number of iterations of a same size", cxxopts::value<int>(num_iteration)->default_value("1"))
            ("m,modification", "if set, run our modified version", cxxopts::value<int>(adgMod::MOD)->default_value("0"))
            ("h,help", "print help message", cxxopts::value<bool>()->default_value("false"))
            ("d,directory", "the directory of db", cxxopts::value<string>(db_location)->default_value("./db"))
            ("k,key_size", "the size of key", cxxopts::value<int>(adgMod::key_size)->default_value("8"))
            ("v,value_size", "the size of value", cxxopts::value<int>(adgMod::value_size)->default_value("8"))
            ("single_timing", "print the time of every single get", cxxopts::value<bool>(print_single_timing)->default_value("false"))
            ("file_info", "print the file structure info", cxxopts::value<bool>(print_file_info)->default_value("false"))
            ("test_num_segments", "test: number of segments per level", cxxopts::value<float>(test_num_segments_base)->default_value("1"))
            ("string_mode", "test: use string or int in model", cxxopts::value<bool>(adgMod::string_mode)->default_value("false"))
            ("e,model_error", "error in modesl", cxxopts::value<uint32_t>(adgMod::model_error)->default_value("8"))
            ("f,input_file", "the filename of input file", cxxopts::value<string>(input_filename)->default_value(""))
            ("multiple", "test: use larger keys", cxxopts::value<uint64_t>(adgMod::key_multiple)->default_value("1"))
            ("w,write", "writedb", cxxopts::value<bool>(fresh_write)->default_value("false"))
            ("c,uncache", "evict cache", cxxopts::value<bool>(evict)->default_value("false"))
            ("u,unlimit_fd", "unlimit fd", cxxopts::value<bool>(unlimit_fd)->default_value("false"))
            ("x,dummy", "dummy option")
            ("l,load_type", "load type", cxxopts::value<int>(load_type)->default_value("0"))
            ("filter", "use filter", cxxopts::value<bool>(adgMod::use_filter)->default_value("false"))
            ("mix", "mix read and write", cxxopts::value<int>(num_mix)->default_value("0"))
            ("distribution", "operation distribution", cxxopts::value<string>(distribution_filename)->default_value(""))
            ("change_level_load", "load level model", cxxopts::value<bool>(change_level_load)->default_value("false"))
            ("change_file_load", "enable level learning", cxxopts::value<bool>(change_file_load)->default_value("false"))
            ("change_level_learning", "load file model", cxxopts::value<bool>(change_level_learning)->default_value("false"))
            ("change_file_learning", "enable file learning", cxxopts::value<bool>(change_file_learning)->default_value("false"))
            ("p,pause", "pause between operation", cxxopts::value<bool>(pause)->default_value("false"))
            ("policy", "learn policy", cxxopts::value<int>(adgMod::policy)->default_value("0"))
            ("YCSB", "use YCSB trace", cxxopts::value<string>(ycsb_filename)->default_value(""))
            ("insert", "insert new value", cxxopts::value<int>(insert_bound)->default_value("0"))
            ("range", "use range query and specify length", cxxopts::value<int>(length_range)->default_value("0"));
    auto result = commandline_options.parse(argc, argv);
    if (result.count("help")) {
        printf("%s", commandline_options.help().c_str());
        exit(0);
    }
    
    std::default_random_engine e1(0), e2(255), e3(0);
    srand(0);
    num_operations *= num_pairs_base;
    db_location_copy = db_location;

    adgMod::fd_limit = unlimit_fd ? 1024 * 1024 : 1024;
    adgMod::restart_read = true;
    adgMod::level_learning_enabled ^= change_level_learning;
    adgMod::file_learning_enabled ^= change_file_learning;
    adgMod::load_level_model ^= change_level_load;
    adgMod::load_file_model ^= change_file_load;

   // adgMod::file_learning_enabled = false;

    
    vector<string> keys;
    vector<uint64_t> distribution;
    vector<int> ycsb_is_write;
    //keys.reserve(100000000000 / adgMod::value_size);
    if (!input_filename.empty()) {
        ifstream input(input_filename);
        string key;
        while (input >> key) {
            string the_key = generate_key(key);
            keys.push_back(std::move(the_key));
        }
        //adgMod::key_size = (int) keys.front().size();
    } else {
        std::uniform_int_distribution<uint64_t> udist_key(0, 999999999999999);
        for (int i = 0; i < 10000000; ++i) {
            keys.push_back(generate_key(to_string(udist_key(e2))));
        }
    }
    



    
    bool copy_out = num_mix != 0 || use_ycsb;

    adgMod::Stats* instance = adgMod::Stats::GetInstance();
    vector<vector<size_t>> times(20);
    string values(1024 * 1024, '0');



    db_location = db_location_copy;
    std::uniform_int_distribution<uint64_t > uniform_dist_file(0, (uint64_t) keys.size() - 1);
    std::uniform_int_distribution<uint64_t > uniform_dist_file2(0, (uint64_t) keys.size() - 1);
    std::uniform_int_distribution<uint64_t > uniform_dist_value(0, (uint64_t) values.size() - adgMod::value_size - 1);

    DB* db;
    Options options;
    ReadOptions& read_options = adgMod::read_options;
    WriteOptions& write_options = adgMod::write_options;
    Status status;

    options.create_if_missing = true;
    write_options.sync = false;
    instance->ResetAll();

    int iteration = 0;


    cout << "Starting up" << endl;
    status = DB::Open(options, db_location, &db);
    adgMod::db->WaitForBackground();
    Iterator* db_iter = length_range == 0 ? nullptr : db->NewIterator(read_options);
    assert(status.ok() && "Open Error");


    // New code from here -- Wenlong






    // New code ends here


    /*
    uint64_t last_read = 0, last_write = 0;
    int last_level = 0, last_file = 0, last_baseline = 0, last_succeeded = 0, last_false = 0, last_compaction = 0, last_learn = 0;
    std::vector<uint64_t> detailed_times;
    bool start_new_event = true;

    instance->StartTimer(13);
    uint64_t write_i = 0;
    for (int i = 0; i < num_operations; ++i) {

        if (start_new_event) {
            detailed_times.push_back(instance->GetTime());
            start_new_event = false;
        }

        bool write = use_ycsb ? ycsb_is_write[i] == 1 || ycsb_is_write[i] == 2 : (i % mix_base) < num_mix;
        length_range = use_ycsb && ycsb_is_write[i] > 2 ? ycsb_is_write[i] - 100 : length_range;

        if (write) {
            if (input_filename.empty()) {
                instance->StartTimer(10);
                status = db->Put(write_options, generate_key(to_string(distribution[i])), {values.data() + uniform_dist_value(e3), (uint64_t) adgMod::value_size});
                instance->PauseTimer(10);
            } else {
                uint64_t index;
                if (use_distribution) {
                    index = distribution[i];
                } else if (load_type == 0) {
                    index = write_i++ % keys.size();
                } else {
                    index = uniform_dist_file(e1) % (keys.size() - 1);
                }

                instance->StartTimer(10);
                if (use_ycsb && ycsb_is_write[i] == 2) {
                    status = db->Put(write_options, generate_key(to_string(10000000000 + index)), {values.data() + uniform_dist_value(e3), (uint64_t) adgMod::value_size});
                } else {
                    status = db->Put(write_options, keys[index], {values.data() + uniform_dist_value(e3), (uint64_t) adgMod::value_size});
                }
                instance->PauseTimer(10);
                assert(status.ok() && "Mix Put Error");
                //cout << index << endl;
            }
        } else if (length_range != 0) {
            // Seek
            if (input_filename.empty()) {
                instance->StartTimer(4);
                db_iter->Seek(generate_key(to_string(distribution[i])));
                instance->PauseTimer(4);
            } else {
                uint64_t index = use_distribution ? distribution[i] : uniform_dist_file2(e2) % (keys.size() - 1);
                index = index >= length_range ? index - length_range : 0;
                const string& key = keys[index];
                instance->StartTimer(4);
                db_iter->Seek(key);
                instance->PauseTimer(4);
            }
            
            // Range
            instance->StartTimer(17);
            for (int r = 0; r < length_range; ++r) {
                if (!db_iter->Valid()) break;
                Slice key = db_iter->key();
                string value = db_iter->value().ToString();
                // cout << key.ToString() << value << endl;
                // value.clear();
                db_iter->Next();
            }
            instance->PauseTimer(17);
        } else {
            string value;
            if (input_filename.empty()) {
                instance->StartTimer(4);
                status = db->Get(read_options, generate_key(to_string(distribution[i])), &value);
                instance->PauseTimer(4);
                if (!status.ok()) {
                    cout << distribution[i] << " Not Found" << endl;
                    //assert(status.ok() && "File Get Error");
                }
            } else {
                uint64_t index = use_distribution ? distribution[i] : uniform_dist_file2(e2) % (keys.size() - 1);
                const string& key = keys[index];
                instance->StartTimer(4);
                if (insert_bound != 0 && index > insert_bound) {
                    status = db->Get(read_options, generate_key(to_string(10000000000 + index)), &value);
                } else {
                    status = db->Get(read_options, key, &value);
                }
                instance->PauseTimer(4);

                //cout << "Get " << key << " : " << value << endl;
                if (!status.ok()) {
                    cout << key << " Not Found" << endl;
                    //assert(status.ok() && "File Get Error");
                }
            }
        }

        if (pause) {
            if ((i + 1) % (num_operations / 10000) == 0) ::usleep(800000);
        }

        if ((i + 1) % (num_operations / 100) == 0) detailed_times.push_back(instance->GetTime());
        if ((i + 1) % (num_operations / 10) == 0) {
            int level_read = levelled_counters[0].Sum();
            int file_read = levelled_counters[1].Sum();
            int baseline_read = levelled_counters[2].Sum();
            int succeeded_read = levelled_counters[3].NumSum();
            int false_read = levelled_counters[4].NumSum();

            compaction_counter_mutex.Lock();
            int num_compaction = events[0].size();
            compaction_counter_mutex.Unlock();
            learn_counter_mutex.Lock();
            int num_learn = events[1].size();
            learn_counter_mutex.Unlock();

            uint64_t read_time = instance->ReportTime(4);
            uint64_t write_time = instance->ReportTime(10);
            std::pair<uint64_t, uint64_t> time = {detailed_times.front(), detailed_times.back()};

            events[2].push_back(new WorkloadEvent(time, level_read - last_level, file_read - last_file, baseline_read - last_baseline,
                succeeded_read - last_succeeded, false_read - last_false, num_compaction - last_compaction, num_learn - last_learn,
                read_time - last_read, write_time - last_write, std::move(detailed_times)));

            last_level = level_read;
            last_file = file_read;
            last_baseline = baseline_read;
            last_succeeded = succeeded_read;
            last_false = false_read;
            last_compaction = num_compaction;
            last_learn = num_learn;
            last_read = read_time;
            last_write = write_time;
            detailed_times.clear();
            start_new_event = true;
            cout << (i + 1) / (num_operations / 10) << endl;
            Version* current = adgMod::db->versions_->current();
            printf("LevelSize %d %d %d %d %d %d\n", current->NumFiles(0), current->NumFiles(1), current->NumFiles(2), current->NumFiles(3),
                    current->NumFiles(4), current->NumFiles(5));
        }

    }
    instance->PauseTimer(13, true);


    instance->ReportTime();
    for (int s = 0; s < times.size(); ++s) {
        times[s].push_back(instance->ReportTime(s));
    }
    adgMod::db->WaitForBackground();
    sleep(10);



    for (auto& event_array : events) {
        for (Event* e : event_array) e->Report();
    }

    for (Counter& c : levelled_counters) c.Report();

    file_data->Report();

    for (auto it : file_stats) {
        printf("FileStats %d %d %lu %lu %u %u %lu %d\n", it.first, it.second.level, it.second.start,
            it.second.end, it.second.num_lookup_pos, it.second.num_lookup_neg, it.second.size, it.first < file_data->watermark ? 0 : 1);
    }

    adgMod::learn_cb_model->Report();

    delete db_iter;
    delete db;

    /*
    for (int s = 0; s < times.size(); ++s) {
        vector<uint64_t>& time = times[s];
        vector<double> diff(time.size());
        if (time.empty()) continue;

        double sum = std::accumulate(time.begin(), time.end(), 0.0);
        double mean = sum / time.size();
        std::transform(time.begin(), time.end(), diff.begin(), [mean] (double x) { return x - mean; });
        double stdev = std::sqrt(std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0) / time.size());

        printf("Timer %d MEAN: %lu, STDDEV: %f\n", s, (uint64_t) mean, stdev);
    }

    if (num_iteration > 1) {
        cout << "Data Without the First Item" << endl;
        for (int s = 0; s < times.size(); ++s) {
            vector<uint64_t>& time = times[s];
            vector<double> diff(time.size() - 1);
            if (time.empty()) continue;

            double sum = std::accumulate(time.begin() + 1, time.end(), 0.0);
            double mean = sum / (time.size() - 1);
            std::transform(time.begin() + 1, time.end(), diff.begin(), [mean] (double x) { return x - mean; });
            double stdev = std::sqrt(std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0) / time.size());

            printf("Timer %d MEAN: %lu, STDDEV: %f\n", s, (uint64_t) mean, stdev);
        }
    }
    */
}