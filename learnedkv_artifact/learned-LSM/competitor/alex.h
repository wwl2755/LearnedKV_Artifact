#include "./LearnedIndexDiskExp/code/ALEX/src/core/alex.h"

using namespace std;

template<class KEY_TYPE, class PAYLOAD_TYPE>
class Alex_competitor{
public:
    Alex_competitor(int hybrid_mode, char *index_name, char *inner_file):
        index(hybrid_mode, true, index_name, inner_file){}
    


    void bulk_load (const pair<KEY_TYPE,PAYLOAD_TYPE> values[], int num_keys){
        index.bulk_load(values, num_keys);
        index.sync_metanode(true);
        index.sync_metanode(false);
    }

    void get(KEY_TYPE key, PAYLOAD_TYPE* value){
        int tmp = 0;
        index.get_leaf_disk(key, value, &tmp, &tmp, &tmp);
        index.sync_metanode(true);
        index.sync_metanode(false);
    }

    void put(KEY_TYPE key, PAYLOAD_TYPE value){
        long long tmp1 = 0;
        long long tmp2 = 0;
        long long tmp3 = 0;
        long long tmp4 = 0;
        int tmp5 = 0;
        index.insert_disk(key, value, &tmp1,&tmp2,&tmp3,&tmp4,&tmp5);
        index.sync_metanode(true);
        index.sync_metanode(false);
    }
private:
    alex::Alex<KEY_TYPE, PAYLOAD_TYPE> index;
};

