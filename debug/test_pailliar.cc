#include "parser/sql_utils.hh"
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "main/CryptoHandlers.hh"
#include "crypto/arc4.hh"
#include "wrapper/reuse.hh"
#include "wrapper/common.hh"
#include "wrapper/insert_lib.hh"
#include "util/constants.hh"
#include "util/timer.hh"
#include "util/log.hh"
#include "util/util.hh"
#include <NTL/ZZ.h>

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;

using namespace NTL;

int
main(int argc,char**argv) {
    Paillier_priv * sk;
    uint nbits = 1024;
    const std::unique_ptr<streamrng<arc4>>
        prng(new streamrng<arc4>("123456"));
    sk = new Paillier_priv(Paillier_priv::keygen(prng.get(), nbits));
    NTL::ZZ k = sk->hompubkey();

    ZZ pt0 = NTL::to_ZZ(1);
    ZZ pt1 = NTL::to_ZZ(2);

    const ZZ enc0 = sk->encrypt(pt0);
    const ZZ dec0 = sk->decrypt(enc0);
    const ZZ enc1 = sk->encrypt(pt1);
    const ZZ dec1 = sk->decrypt(enc1);
    assert(pt0==dec0);
    assert(pt1==dec1);
    assert((pt0+pt1)==sk->decrypt(sk->add(enc0,enc1)));

    std::cout<<sk->decrypt(MulMod(enc0,enc1,k))<<std::endl;

    UNUSED(sk);
    UNUSED(k);

    return 0;
}
