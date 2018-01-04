#pragma once

#include <string>
#include "crypto/blowfish.hh"
class ASHE{
    static const unsigned long ASHE_MAX;
    std::string key;
    blowfish bf;
    uint64_t IV;
public:
    ASHE(std::string s,int i);
    std::pair<long,uint64_t> encrypt(unsigned long plaintext);
    int getIV();
    unsigned long decrypt(long ciphertext);
    static std::pair<long,std::vector<uint64_t>> sum(std::vector<std::pair<long,uint64_t>> input);
    static unsigned long decrypt_sum(std::pair<long,std::vector<uint64_t>> input);
};
