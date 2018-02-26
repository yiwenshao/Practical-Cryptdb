#pragma once

#include <string>
#include "crypto/blowfish.hh"

class RAW_ASHE{
    static const unsigned int RAW_ASHE_MAX;/*n*/
    static const std::string key;
    static blowfish bf;
    uint64_t IV;
    long ciphertext;
public:
    RAW_ASHE(int iv);

    long get_ciphertext(){return ciphertext;}

    std::pair<long,uint64_t> encrypt(unsigned int plaintext);
    std::pair<long,uint64_t> encrypt(unsigned int plaintext,uint64_t inIv);

    unsigned int decrypt(long ciphertext);
    unsigned int decrypt(long ciphertext,uint64_t inIv);
    uint64_t get_IV(){return IV;};

    static uint64_t Fi(uint64_t IV) {return bf.encrypt(IV)%RAW_ASHE_MAX;}
    static uint64_t Fi_1(uint64_t IV) {return bf.encrypt(IV-1)%RAW_ASHE_MAX;}
    static std::pair<long,std::vector<uint64_t>> sum(
                              std::pair<long,std::vector<uint64_t>> left,
                              std::pair<long,std::vector<uint64_t>> right);
    static std::pair<long,std::vector<uint64_t>> sum(std::vector<RAW_ASHE>);
    static uint64_t decrypt_sum(std::pair<long,std::vector<uint64_t>>);
};



