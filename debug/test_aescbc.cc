#include <string>
#include <map>
#include <iostream>







#include <functional>
#include <cctype>
#include <locale>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>








#include <main/macro_util.hh>
#include <main/CryptoHandlers.hh>



#include <parser/lex_util.hh>

#include <readline/readline.h>
#include <readline/history.h>

#include <crypto/ecjoin.hh>
#include <crypto/search.hh>
#include <crypto/padding.hh>
#include <util/errstream.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <util/yield.hpp>
#include "util/onions.hh"

#include <util/cryptdb_log.hh>
#include <crypto/pbkdf2.hh>
#include <crypto/ECJoin.hh>
#include <crypto/skip32.hh>

#include <vector>
#include <iomanip>


#include <crypto/prng.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/paillier.hh>
#include <crypto/ope.hh>
#include <crypto/blowfish.hh>
#include <parser/sql_utils.hh>
#include <crypto/SWPSearch.hh>
#include <crypto/ope.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/SWPSearch.hh>
#include <crypto/arc4.hh>
#include <crypto/hgd.hh>
#include <crypto/mont.hh>
#include <crypto/cbc.hh>
#include <crypto/cmc.hh>
#include <crypto/gfe.hh>
#include <util/util.hh>
#include <util/cryptdb_log.hh>
#include <util/zz.hh>
#include <cmath>
#include <NTL/ZZ.h>
#include <NTL/RR.h>
#include <sstream>
#include <map>

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
//    cout<<"ptext len = "<<ptext.size()<<" : "<<"key len = "<<rawkey.size()<<endl;
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

