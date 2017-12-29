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
#include <util/util.hh>
#include <NTL/ZZ.h>


using namespace NTL;

using std::string;
using std::cout;
using std::cin;
using std::endl;
using std::make_pair;
using std::setw;
using std::setfill;
using std::hex;
using std::dec;



static std::string prng_expand(const std::string &seed_key, uint key_bytes)
{   
    streamrng<arc4> prng(seed_key);
    return prng.rand_string(key_bytes);
}


static uint64_t cur_usec() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
}



static void test_RNDint(){
    string key="123456789";
    blowfish bf(key); 
    uint64_t IV = 1234567;
    uint64_t ptext = 123;
    const uint64_t c = bf.encrypt(ptext ^ IV);
    const uint64_t p = bf.decrypt(c) ^ IV;
    cout<<c<<":"<<p<<endl;
}


static string generateStringOfLen(int len){
    return string(len,'a');
}

static void test_RNDstr(string ptext,int numOfTest){
    string key="123456789";
    string rawkey = prng_expand(key, 16);
    const std::unique_ptr<const AES_KEY> enckey(get_AES_enc_key(rawkey));
    const std::unique_ptr<const AES_KEY> deckey(get_AES_dec_key(rawkey));
    uint64_t IV = 1234567;
    uint64_t start = cur_usec();
    string enc,dec;
    for(int i=1;i<=numOfTest;i++){
        enc = encrypt_AES_CBC(ptext,enckey.get(),BytesFromInt(IV, SALT_LEN_BYTES),true);
    }
    uint64_t end = cur_usec();
    cout<<"encrypt_AES_CBC:\t"<<(end-start)*1.0/numOfTest<<"\t";
    start = cur_usec();
    for(int i=1;i<=numOfTest;i++){
        dec = decrypt_AES_CBC(enc,deckey.get(),BytesFromInt(IV, SALT_LEN_BYTES),true);
    }
    end = cur_usec();
    cout<<"decrypt_AES_CBC:\t"<<(end - start)*1.0/numOfTest<<endl;
}


int
main(int argc,char** argv) {
    if(argc!=2){
        cout<<"please input numof tests"<<endl;
        return 0;
    }
    int numOfTest = std::stoi(argv[1]);

    int blockSize=16;
    for(int i=1;i<500;i+=10){
        cout<<"i:\t"<<i<<"\t";
        test_RNDstr(generateStringOfLen(i*blockSize),numOfTest);
    }
    test_RNDint();
    return 0;
}

