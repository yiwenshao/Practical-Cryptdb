#include "crypto/ASHE.hh"
#include <iostream>
const unsigned int ASHE::ASHE_MAX = 0xffffffff;
const std::string ASHE::key("11223344");
blowfish ASHE::bf(ASHE::key);

ASHE::ASHE(int i):IV(i){
}

std::pair<long,uint64_t> ASHE::encrypt(unsigned int plaintext){
    uint64_t i = Fi(IV)%ASHE_MAX, i_1=Fi_1(IV)%ASHE_MAX;    
    long res = (long)i_1 - (long)i;
    ciphertext = ((long)plaintext + res)%ASHE_MAX;
    return std::make_pair(ciphertext,IV);
}

unsigned int ASHE::decrypt(long ciphertext){
    uint64_t i = Fi(IV)%ASHE_MAX, i_1=Fi_1(IV)%ASHE_MAX;
    long res = (long)i - (long)i_1;
    return (ciphertext + res)%ASHE_MAX;
}

std::pair<long,std::vector<uint64_t>> ASHE::sum(std::vector<ASHE> input){
    long res=0;
    std::vector<uint64_t> ivs;
    for(auto &item:input){
        long cph = item.get_ciphertext();
        res += cph;
        res %= ASHE_MAX;
        ivs.push_back(item.get_IV());
    }
    return std::make_pair(res,ivs);
}

uint64_t ASHE::decrypt_sum(std::pair<long,std::vector<uint64_t>> input){
    long res = input.first;
    for(auto item:input.second){
        uint64_t i = Fi(item)%ASHE_MAX, i_1=Fi_1(item)%ASHE_MAX;
        long target = (long)i - (long)i_1;
        res += target;
        res %= ASHE_MAX;
    }
    return res;
}

