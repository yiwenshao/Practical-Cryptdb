#include <string>
#include <map>
#include <iostream>
#include <memory>
#include <crypto/padding.hh>
#include <crypto/prng.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/blowfish.hh>
#include <crypto/arc4.hh>
#include <util/util.hh>
#include <NTL/ZZ.h>
#include "util/timer.hh"

int
main(int argc,char**argv){
    blowfish bf("key");
    int num_of_tests = 10000;
    if(argc==2){
        num_of_tests = std::stoi(std::string(argv[1]));
    }else{
        std::cout<<"num_of_tests"<<std::endl;
        return 0;
    }
    uint64_t plain = 111u;
    uint64_t cipher;
    timer t;
    for(int i=0;i<num_of_tests;i++) {
        cipher = bf.encrypt(plain);
    }
    std::cout<<"enc_blowfish_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;
    for(int i=0;i<num_of_tests;i++) {
        bf.decrypt(cipher);
    }
    std::cout<<"dec_blowfish_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;
    return 0;
}
