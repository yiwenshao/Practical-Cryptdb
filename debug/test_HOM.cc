#include <iostream>
#include <sys/time.h>
#include <memory>
#include <iomanip>
#include <crypto/prng.hh>
#include <crypto/paillier.hh>
#include <crypto/arc4.hh>
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

