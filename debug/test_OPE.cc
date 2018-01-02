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

void test_OPEstr(){
    string key = "12345kdljsldajfdls;afjdkals;fjdsal;fdjsal;fdjsalf;djsakfld;sjafdklsa;jfdlksa;jfdslakfjdsal;fjdslkafj6789";
    string rawkey = prng_expand(key, 16);
    const size_t plain_size = 4;
    const size_t ciph_size = 8;       
    OPE ope(rawkey,8*plain_size,8*ciph_size);
    string ptext="helloworld";    
    string ps = toUpperCase(ptext);
    if (ps.size() < plain_size)
        ps = ps + std::string(plain_size - ps.size(), 0);
    
    uint32_t pv = 0;

    for (uint i = 0; i < plain_size; i++) {
        pv = pv * 256 + static_cast<int>(ps[i]);
    }
    const ZZ enc = ope.encrypt(to_ZZ(pv));   
    cout<<"enc: "<<enc<<endl;
}

*/

/*
    not yet supported.

static void
testOPE(){
    unsigned int noSizes = 6;
    unsigned int plaintextSizes[] = {16, 32, 64,  128, 256, 512, 1024};
    unsigned int ciphertextSizes[] = {32, 64, 128, 256, 288, 768, 1536};
    unsigned int noValues = 100;
    string key = "secret aes key!!";
    for (unsigned int i = 0; i < noSizes; i++) {
        unsigned int ptextsize = plaintextSizes[i];
        unsigned int ctextsize =  ciphertextSizes[i];
        OPE * ope = new OPE(string(key, AES_KEY_BYTES), ptextsize, ctextsize);
        //Test it on "noValues" random values
        for (unsigned int j = 0; j < noValues; j++) {
            string data = randomBytes(ptextsize/bitsPerByte);
            //string enc = ope->encrypt(data);
            //string dec = ope->decrypt(enc);
            //assert_s(valFromStr(dec) == valFromStr(data), "decryption does not match original data "  + StringFromVal(ptextsize) + " " + StringFromVal(ctextsize));
        }
    }
}



*/




int
main(int argc,char** argv){
   if(argc!=2){
        std::cout<<"please input numof tests"<<std::endl;
        return 0;
   }
   int numOfTest = std::stoi(argv[1]);      
   test_OPEint(numOfTest);
   return 0;
}
