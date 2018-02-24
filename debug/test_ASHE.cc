#include <vector>
#include <iostream>
#include "crypto/ASHE.hh"
#include "util/util.cc"
static
void test1() {
    const unsigned int num_of_tests = 0xffffffff;
    unsigned int seed = 4294967294u;
    std::vector<unsigned int> plain;
    std::vector<RAW_ASHE> ass;
    for(unsigned int i=0u;i<num_of_tests;i++){
        plain.push_back(seed);
        uint64_t IV = randomValue();
        if(IV==0) IV=1;
        ass.push_back(RAW_ASHE(IV));
        ass.back().encrypt(seed,IV);
        unsigned int res = ass.back().decrypt(ass.back().get_ciphertext(),IV);
        if(res==seed) ;
        else{
            std::cout<<"not pass!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<seed<<std::endl;
//            return 0;
        }
        seed++;
    }
//    std::pair<long,std::vector<uint64_t>> enc_sum = RAW_ASHE::sum(ass);
//    long res = RAW_ASHE::decrypt_sum(enc_sum);
//    std::cout<<enc_sum.first<<"::"<<res<<std::endl;

}

static
void test2(){
    const unsigned int num_of_tests = 0xffffffff;
    unsigned int seed = 0u;
    for(unsigned int i=0u;i<num_of_tests;i++){
        uint64_t IV = randomValue();
        if(IV==0) IV=1;
        RAW_ASHE ashe(IV);
        auto enc = ashe.encrypt(seed,IV);
        assert(enc.first == ashe.get_ciphertext());
        assert(enc.second == IV);
        unsigned int res = ashe.decrypt(enc.first,enc.second);
        if(res==seed) ;
        else{
            std::cout<<"not pass!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<seed<<std::endl;
        }
        seed++;
    }
}



int main(){
    UNUSED(test1);
    test2();
    return 0;
}

