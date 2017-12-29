#include <vector>
#include <iostream>
#include "crypto/ASHE.hh"
#include "util/util.cc"

int main(){
    std::vector<unsigned long long > plain{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u};
    std::vector<long> enc;
    std::vector<ASHE*> ass;
    for(auto item:plain){
        uint64_t IV = randomValue();
        if(IV==0) IV=1;
        ass.push_back(new ASHE("111",IV));
        enc.push_back(ass.back()->encrypt(item));
    }
    std::cout<<"encs:plains"<<std::endl;
    for(auto i=0u;i<enc.size();++i){
        std::cout<<"enc:"<<enc[i]<<"dec:"<<ass[i]->decrypt(enc[i])<<std::endl;
    }
    return 0;
}

