#include <string>
#include <map>
#include <iostream>
#include <memory>
#include <iomanip>
#include <crypto/padding.hh>
#include <crypto/prng.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/blowfish.hh>
#include <crypto/arc4.hh>
#include <crypto/cbc.hh>
#include <crypto/ope.hh>
#include <util/util.hh>
#include <NTL/ZZ.h>
#include "util/timer.hh"

using namespace NTL;
static std::string prng_expand(const std::string &seed_key, uint key_bytes){
    streamrng<arc4> prng(seed_key);
    return prng.rand_string(key_bytes);
}
int
main(int argc,char**argv){
    int num_of_tests = 10000;
    if(argc==2){
        num_of_tests = std::stoi(std::string(argv[1]));
    }else{
        std::cout<<"num_of_tests"<<std::endl;
        return 0;
    }
    std::string key = "12345798797";
    std::string rawkey = prng_expand(key, 16);
    const size_t plain_size = 4;
    const size_t ciph_size = 8;
    OPE ope(rawkey,8*plain_size,8*ciph_size);
    std::string ps = "1234567890";
    timer t;   
    for(int i=0;i<num_of_tests;i++) {
        uint32_t pv = 0;
        for (uint i = 0; i < plain_size; i++) {
            pv = pv * 256 + static_cast<int>(ps[i]);
        }
        ope.encrypt(to_ZZ(pv));
    }
    std::cout<<"enc_ope_str_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;
    return 0;
}
