#include <vector>
#include <iostream>
#include "crypto/ASHE.hh"
#include "util/util.cc"

int main(){
    std::vector<unsigned long long > plain{1u,2u,3u//,4u,5u,6u,7u,8u,9u,10u
                                          };
    std::vector<ASHE> ass;
    for(auto item:plain){
        uint64_t IV = randomValue();
        if(IV==0) IV=1;
        ass.push_back(ASHE(IV));
        ass.back().encrypt(item);
    }
    
    for(auto &item:ass){
        std::cout<<item.get_ciphertext()<<"::"<<item.decrypt(item.get_ciphertext())<<std::endl;
    }

    std::pair<long,std::vector<uint64_t>> enc_sum = ASHE::sum(ass);

    uint64_t res = ASHE::decrypt_sum(enc_sum);
    std::cout<<enc_sum.first<<"::"<<res<<std::endl;
    return 0;
}

