#include "util/onions.hh"
#include <assert.h>

static
void test_is_onionlayout_equal(onionlayout &ol1,onionlayout &ol2){
    for(auto item:ol1){
        auto key = item.first;
        assert(ol2.find(key)!=ol2.end());
        assert(ol2[key]==ol1[key]);
    }
}


int main(){
    const char *di = "conf/CURRENT.conf";
    onion_conf of(di);
    auto res = of.get_onionlayout_for_num();
    auto res2 = of.get_onionlayout_for_str();
    test_is_onionlayout_equal(res,NUM_ONION_LAYOUT);
    test_is_onionlayout_equal(NUM_ONION_LAYOUT,res);

    test_is_onionlayout_equal(res2,STR_ONION_LAYOUT);
    test_is_onionlayout_equal(STR_ONION_LAYOUT,res2);
    return 0;
}

