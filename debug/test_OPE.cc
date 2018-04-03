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

using namespace NTL;

static std::string prng_expand(const std::string &seed_key, uint key_bytes){
    streamrng<arc4> prng(seed_key);
    return prng.rand_string(key_bytes);
}


static
void test_OPEint(int numOfTest){
    std::string key = "12345798797";
    std::string rawkey = prng_expand(key, 16);
    const size_t plain_size = 4;
    const size_t ciph_size = 8;
    OPE ope(rawkey,8*plain_size,8*ciph_size);
    uint64_t plaintext = 123456789;
    NTL::ZZ enc,dec;
    uint64_t enc64,dec64;

    for(int i=0;i<numOfTest;i++){
        enc = ope.encrypt(ZZFromUint64(plaintext));
        enc64 = uint64FromZZ(enc);
    }

    for(int i=0;i<numOfTest;i++){
        dec = ope.decrypt(ZZFromUint64(enc64));
        dec64 = uint64FromZZ(dec);
    }

    std::cout<<"enc: "<<enc<<"dec: "<<dec<<std::endl;
    std::cout<<"enc64: "<<enc64<<"dec64:"<<dec64<<std::endl;
}



/*
only encrypt, no decrypt.
*/
static
void test_OPEstr(){
    std::string key = "12345k";
    std::string rawkey = prng_expand(key, 16);
    const size_t plain_size = 4;
    const size_t ciph_size = 8;       
    OPE ope(rawkey,8*plain_size,8*ciph_size);

    std::string ptext="helloworld";    
    std::string ps = toUpperCase(ptext);
    if (ps.size() < plain_size)
        ps = ps + std::string(plain_size - ps.size(), 0);
    
    uint32_t pv = 0;

    for (uint i = 0; i < plain_size; i++) {
        pv = pv * 256 + static_cast<int>(ps[i]);
    }
    /*strcomp by prefix*/
    const ZZ enc = ope.encrypt(to_ZZ(pv));
    std::cout<<"enc: "<<enc<<std::endl;
}

int
main(int argc,char** argv){
   int numOfTest = 1;
   test_OPEint(numOfTest);
   test_OPEstr();
   return 0;
}
