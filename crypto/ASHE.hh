#pragma once

#include <string>
#include "crypto/blowfish.hh"

class ASHE{
    static const unsigned long ASHE_MAX;
    static const std::string key;
    static blowfish bf;
    uint64_t IV;
    long ciphertext;
public:

    static uint64_t Fi(uint64_t IV){return bf.encrypt(IV);}
    static uint64_t Fi_1(uint64_t IV){return bf.encrypt(IV-1);}
    long get_ciphertext(){return ciphertext;}
    ASHE(int iv);

    std::pair<long,uint64_t> encrypt(unsigned long plaintext);
    uint64_t get_IV(){return IV;};
    unsigned long decrypt(long ciphertext);
    
    static std::pair<long,std::vector<uint64_t>> sum(std::vector<ASHE>);
    static uint64_t decrypt_sum(std::pair<long,std::vector<uint64_t>>);
};
