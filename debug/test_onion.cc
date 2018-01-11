#include "util/onions.hh"

int main(){
    char *dir = (char*)"/home/casualet/github/Practical-Cryptdb/onionlayout.conf";
    onion_conf of(dir);
    auto res = of.get_onionlayout_for_num();
    auto res2 = of.get_onion_levels_str();
    return 0;
}
