#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <main/Connect.hh>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>
#include <main/CryptoHandlers.hh>

#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
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

static const int numOfTest = 100;


static uint64_t cur_usec() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
}


/*static void test_paillier_priv(){
    Paillier_priv * sk;
    string seed_key="1234567678";
    const std::unique_ptr<streamrng<arc4>> prng(new streamrng<arc4>(seed_key));
    urandom u;
    sk = new Paillier_priv(Paillier_priv::keygen(prng.get(), 1024));
    for(int i=0;i<1024;i++){
        ZZ pt0 = u.rand_zz_mod(to_ZZ(1) << 20);
        ZZ pt1 = u.rand_zz_mod(to_ZZ(1) << 20);
        const ZZ enc0 = sk->encrypt(pt0);
        const ZZ dec0 = sk->decrypt(enc0);
        const ZZ enc1 = sk->encrypt(pt1);
        const ZZ dec1 = sk->decrypt(enc1);
        assert(pt0==dec0);
        assert(pt1==dec1);
        assert((pt0+pt1)==sk->decrypt(sk->add(enc0,enc1)));
        cout<<"PASS"<<endl;
    }

}*/

static void test_paillier_time() {
    Paillier_priv * sk;
    string seed_key="1234567678";
    const std::unique_ptr<streamrng<arc4>> prng(new streamrng<arc4>(seed_key));
    urandom u;
    sk = new Paillier_priv(Paillier_priv::keygen(prng.get(), 1024));
    ZZ pt0 = u.rand_zz_mod(to_ZZ(1) << 20);

    cout<<"numOfTests: "<<numOfTest<<endl;
    ZZ enc0;
    uint64_t start = cur_usec();
    for(int i=0;i<numOfTest;i++){
        enc0 = sk->encrypt(pt0);
    }
    uint64_t end = cur_usec();

    cout<<"encryption: "<<(end-start)*1.0/numOfTest<<endl;

    start = cur_usec();
    for(int i=0;i<numOfTest;i++){
        const ZZ dec0 = sk->decrypt(enc0);
    }
    end = cur_usec();
    cout<<"decryption: "<<(end-start)*1.0/numOfTest<<endl;
}



int
main() {
    test_paillier_time();
    return 0;
}

