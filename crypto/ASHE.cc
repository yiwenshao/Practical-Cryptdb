#include"crypto/ASHE.hh"
const unsigned long ASHE::ASHE_MAX = 0xffffffffffffffff;

ASHE::ASHE(std::string s,int i):key(s),bf(s),IV(i){

}

std::pair<long,uint64_t> ASHE::encrypt(unsigned long plaintext){
    return std::make_pair((plaintext - bf.encrypt(IV) + bf.encrypt(IV-1))%ASHE_MAX,IV);

}

unsigned long ASHE::decrypt(long ciphertext){
    return (ciphertext +  bf.encrypt(IV) - bf.encrypt(IV-1))%ASHE_MAX;
}
