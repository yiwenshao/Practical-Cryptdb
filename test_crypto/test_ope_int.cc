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
void test_OPEint() {
    std::string key = "12345798797";
    std::string rawkey = prng_expand(key, 16);
    const size_t plain_size = 4;
    const size_t ciph_size = 8;
    OPE ope(rawkey,8*plain_size,8*ciph_size);
    uint64_t plaintext = 123456789;
    NTL::ZZ enc,dec;
    uint64_t enc64,dec64;
    enc = ope.encrypt(ZZFromUint64(plaintext));
    enc64 = uint64FromZZ(enc);
    dec = ope.decrypt(ZZFromUint64(enc64));
    dec64 = uint64FromZZ(dec);
    std::cout<<"enc: "<<enc<<"dec: "<<dec<<std::endl;
    std::cout<<"enc64: "<<enc64<<"dec64:"<<dec64<<std::endl;
}

int
main(){
   test_OPEint();
   return 0;
}
