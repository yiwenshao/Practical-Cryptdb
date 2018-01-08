#include"crypto/ASHE.hh"
const unsigned long ASHE::ASHE_MAX = 0xffffffffffffffff;
const std::string ASHE::key("11223344");
blowfish ASHE::bf(ASHE::key);


ASHE::ASHE(int i):IV(i){
}

std::pair<long,uint64_t> ASHE::encrypt(unsigned long plaintext){
    ciphertext = (plaintext - Fi(IV) + Fi_1(IV))%ASHE_MAX;
    return std::make_pair(ciphertext,IV);
}

unsigned long ASHE::decrypt(long ciphertext){
    return (ciphertext +  Fi(IV) - Fi_1(IV))%ASHE_MAX;
}



std::pair<long,std::vector<uint64_t>> ASHE::sum(std::vector<ASHE> input){
    long res=0;
    std::vector<uint64_t> ivs;
    for(auto &item:input){
        res += item.get_ciphertext();
        res %= ASHE_MAX;
        ivs.push_back(item.get_IV());
    }
    return std::make_pair(res,ivs);
}

uint64_t ASHE::decrypt_sum(std::pair<long,std::vector<uint64_t>> input){
    uint64_t res = input.first;
    for(auto item:input.second){
        res += (Fi(item) - Fi_1(item));
        res %= ASHE_MAX;
    }
    return res;
}

