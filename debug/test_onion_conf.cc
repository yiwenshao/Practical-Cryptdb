#include "util/onions.hh"
int main(){
    const char *di = dir;
    onion_conf of(di);
    auto res = of.get_onionlayout_for_num();
    auto res2 = of.get_onion_levels_str();
    return 0;
}
