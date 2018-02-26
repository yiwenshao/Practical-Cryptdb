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
        else {
            std::cout<<"not pass!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<seed<<std::endl;
        }
        seed++;
    }
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
        if(res==seed);
        else{
            std::cout<<"not pass!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<seed<<std::endl;
        }
        seed++;
    }
}

static 
void test3() {
    std::vector<uint64_t> ivs;
    std::vector<long> cipher;
    std::vector<std::pair<long,std::vector<uint64_t>> > agg;
    for(unsigned int i=1;i<=10;i++) {
        uint64_t IV = randomValue();
        if(IV==0) IV=1;
        RAW_ASHE ashe(IV);
        std::pair<long,std::vector<uint64_t>> data = {ashe.encrypt(i,IV).first, {IV}};
        agg.push_back(data);
    }
    auto b = agg[0];
    for(unsigned int i=1;i<agg.size();i++) {
        b=RAW_ASHE::sum(b,agg[i]);
    }
    std::cout<<RAW_ASHE::decrypt_sum(b)<<std::endl;
}

static
void test4() {
    RAW_ASHE ashe1(1);
    RAW_ASHE ashe2(2);
    auto enc1 = ashe1.encrypt(1,1);
    auto enc2 = ashe2.encrypt(2,2);
    std::pair<long,std::vector<uint64_t>> enc1t = {enc1.first,{enc1.second}};
    std::pair<long,std::vector<uint64_t>> enc2t = {enc2.first,{enc2.second}};

    auto encsum = ashe1.sum(enc1t,enc2t);
    std::cout<<ashe1.decrypt_sum(encsum)<<std::endl;
}

int main(){
    UNUSED(test1);
    UNUSED(test2);
    test3();
    test4();
    return 0;
}

