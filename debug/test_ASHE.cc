#include <vector>
#include <iostream>
#include "crypto/ASHE.hh"
#include "util/util.cc"
int main(){
    const int num_of_tests = 32768;
    unsigned int seed = 1u;
    std::vector<unsigned int> plain;
    std::vector<RAW_ASHE> ass;
    for(int i=0;i<num_of_tests;i++){
        plain.push_back(seed);
        uint64_t IV = randomValue();
        if(IV==0) IV=1;
        ass.push_back(RAW_ASHE(IV));
        ass.back().encrypt(seed,IV);
        unsigned int res = ass.back().decrypt(ass.back().get_ciphertext(),IV);
        if(res==seed) ;
        else std::cout<<"not pass!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
        seed++;
    }

    std::pair<long,std::vector<uint64_t>> enc_sum = RAW_ASHE::sum(ass);
    long res = RAW_ASHE::decrypt_sum(enc_sum);
    std::cout<<enc_sum.first<<"::"<<res<<std::endl;
    return 0;
}

