#include <string>
#include <map>
#include <iostream>
#include <memory>
#include <crypto/padding.hh>
#include <crypto/prng.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/blowfish.hh>
#include <crypto/arc4.hh>
#include <crypto/ASHE.hh>
#include <util/util.hh>
#include <NTL/ZZ.h>
#include "util/timer.hh"

int
main(int argc,char**argv){

    RAW_ASHE ashe(1);
    int num_of_tests = 10000;
    if(argc==2){
        num_of_tests = std::stoi(std::string(argv[1]));
    }else{
        std::cout<<"num_of_tests"<<std::endl;
        return 0;
    }

    unsigned int pt = 1u;
    
    timer t;
    std::pair<long,uint64_t> res;
    for(int i=0;i<num_of_tests;i++) {
        res = ashe.encrypt(pt,0);
    }

    std::cout<<"enc_ashe_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;

    for(int i=0;i<num_of_tests;i++) {
        ashe.decrypt(res.first,0);
    }

    std::cout<<"dec_ashe_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;
    return 0;
}
