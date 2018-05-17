#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <vector>

#include "main/CryptoHandlers.hh"
#include "crypto/arc4.hh"
#include "util/timer.hh"
#include "util/util.hh"
#include <NTL/ZZ.h>

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;

using namespace NTL;

static void 
test_pailliar(int num_of_tests) {
    Paillier_priv * sk;
    uint nbits = 1024;
    const std::unique_ptr<streamrng<arc4>>
        prng(new streamrng<arc4>("123456"));
    sk = new Paillier_priv(Paillier_priv::keygen(prng.get(), nbits));
    ZZ pt0 = NTL::to_ZZ(1);
    ZZ enc0;

    timer t;
    for(int i=0;i<num_of_tests;i++){
        enc0 = sk->encrypt(pt0);
    }
    std::cout<<"enc_pailliar_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;

    for(int i=0;i<num_of_tests;i++){
        const ZZ dec0 = sk->decrypt(enc0);
    }
    std::cout<<"dec_pailliar_in_us: "<<t.lap()*1.0/num_of_tests<<std::endl;
}

int
main(int argc,char**argv) {
    int num_of_tests = 10000;
    if(argc==2){
        num_of_tests = std::stoi(std::string(argv[1]));
    }else{
        std::cout<<"num_of_tests"<<std::endl;
        return 0;
    }        
    test_pailliar(num_of_tests);
    return 0;
}
