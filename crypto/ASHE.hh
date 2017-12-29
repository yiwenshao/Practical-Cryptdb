#pragma once

#include <string>
#include "crypto/blowfish.hh"
class ASHE{
    static const unsigned long long ASHE_MAX;
    std::string key;
    blowfish bf;
    uint64_t IV;
public:
    ASHE(std::string s,int i);
    long encrypt(unsigned long long plaintext);
    int getIV();
    unsigned long long decrypt(long ciphertext);
};
