#include "crypto/ASHE.hh"
#include <iostream>
const unsigned int RAW_ASHE::RAW_ASHE_MAX = 0xffffffff;
const std::string RAW_ASHE::key("11223344");
blowfish RAW_ASHE::bf(RAW_ASHE::key);

RAW_ASHE::RAW_ASHE(int i):IV(i){
}

std::pair<long,uint64_t> RAW_ASHE::encrypt(unsigned int plaintext){
    uint64_t i = Fi(IV)%RAW_ASHE_MAX, i_1=Fi_1(IV)%RAW_ASHE_MAX;
    long res = (long)i_1 - (long)i;
    ciphertext = ((long)plaintext + res)%RAW_ASHE_MAX;
    return std::make_pair(ciphertext,IV);
}

std::pair<long,uint64_t> RAW_ASHE::encrypt(unsigned int plaintext,uint64_t inIV){
    uint64_t i = Fi(inIV)%RAW_ASHE_MAX, i_1=Fi_1(inIV)%RAW_ASHE_MAX;
    long offset = (long)i_1 - (long)i;
    ciphertext = ((long)plaintext + offset);
//    std::cout<<"plain:offset:cipher"<<std::endl;
//    std::cout<<plaintext<<":"<<offset<<":"<<ciphertext<<std::endl;
    return std::make_pair(ciphertext,inIV);
}



unsigned int RAW_ASHE::decrypt(long ciphertext){
    uint64_t i = Fi(IV)%RAW_ASHE_MAX, i_1=Fi_1(IV)%RAW_ASHE_MAX;
    long res = (long)i - (long)i_1;
    return (ciphertext + res)%RAW_ASHE_MAX;
}


unsigned int RAW_ASHE::decrypt(long ciphertext,uint64_t inIV){
    uint64_t i = Fi(inIV)%RAW_ASHE_MAX, i_1=Fi_1(inIV)%RAW_ASHE_MAX;
    long offset = (long)i - (long)i_1;
    unsigned int res = (ciphertext + offset);
//    std::cout<<"cipher:offset:plain"<<std::endl;
//    std::cout<<ciphertext<<":"<<offset<<":"<<res<<std::endl;
    return res;
}


std::pair<long,std::vector<uint64_t>> RAW_ASHE::sum(std::vector<RAW_ASHE> input){
    long res=0;
    std::vector<uint64_t> ivs;
    for(auto &item:input){
        long cph = item.get_ciphertext();
        res += cph;
        res %= RAW_ASHE_MAX;
        ivs.push_back(item.get_IV());
    }
    return std::make_pair(res,ivs);
}


std::pair<long,std::vector<uint64_t>> 
RAW_ASHE::sum(std::pair<long,std::vector<uint64_t>> left,
              std::pair<long,std::vector<uint64_t>> right) {
    std::vector<uint64_t> vecsum;
    for(auto item:left.second) vecsum.push_back(item);
    for(auto item:right.second) vecsum.push_back(item);
    return std::make_pair((left.first+right.first)%RAW_ASHE_MAX,vecsum);
}



uint64_t RAW_ASHE::decrypt_sum(std::pair<long,std::vector<uint64_t>> input){
    long res = input.first;
    for(auto item:input.second){
        uint64_t i = Fi(item)%RAW_ASHE_MAX, i_1=Fi_1(item)%RAW_ASHE_MAX;
        long target = (long)i - (long)i_1;
        res += target;
        res %= RAW_ASHE_MAX;
    }
    return res;
}

