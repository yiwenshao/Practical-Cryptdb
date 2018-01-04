#include"crypto/ASHE.hh"
const unsigned long ASHE::ASHE_MAX = 0xffffffffffffffff;

ASHE::ASHE(std::string s,int i):key(s),bf(s),IV(i){

}

std::pair<long,uint64_t> ASHE::encrypt(unsigned long plaintext){
    return std::make_pair((plaintext - bf.encrypt(IV) + bf.encrypt(IV-1))%ASHE_MAX,IV);

}

unsigned long ASHE::decrypt(long ciphertext){
    return (ciphertext +  bf.encrypt(IV) - bf.encrypt(IV-1))%ASHE_MAX;
}

/*Cannot declare member function ...to have static linkage*/
std::pair<long,std::vector<uint64_t>> ASHE::sum(std::vector<std::pair<long,uint64_t>> input){
    std::vector<uint64_t> IVs;
    long res = 0;
    for(auto &item:input){
        res+=item.first;
        IVs.push_back(item.second);
    }
    return std::make_pair(res,IVs);
}

unsigned long ASHE::decrypt_sum(std::pair<long,std::vector<uint64_t>> input){
    long res = input.first;
    for(auto item:input.second){
        item++;
        res++;
    }
    return res;
}



