#include"crypto/ASHE.hh"
const unsigned long long ASHE::ASHE_MAX = 0xffffffffffffffff;

ASHE::ASHE(std::string s,int i):key(s),bf(s),IV(i){

}

long ASHE::encrypt(unsigned long long plaintext){
    return (plaintext - bf.encrypt(IV) + bf.encrypt(IV-1))%ASHE_MAX;   

}

unsigned long long ASHE::decrypt(long ciphertext){
    return (ciphertext +  bf.encrypt(IV) - bf.encrypt(IV-1))%ASHE_MAX;
}
