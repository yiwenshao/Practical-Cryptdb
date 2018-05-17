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

    for(int i=0;i<num_of_tests;i++){
        enc0 = sk->encrypt(pt0);
    }

    for(int i=0;i<num_of_tests;i++){
        const ZZ dec0 = sk->decrypt(enc0);
    }
}


int
main(int argc,char**argv) {
    test_pailliar(100);
    return 0;
}
