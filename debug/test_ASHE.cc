#include <vector>
#include <iostream>
#include "crypto/ASHE.hh"

int main(){
    std::vector<unsigned long long > plain{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u};
    std::vector<long> enc;
    ASHE as("2222",1);
    for(auto item:plain){
        enc.push_back(as.encrypt(item));
    }

    std::cout<<"encs:plains"<<std::endl;
    for(auto item:enc){
        std::cout<<"enc:"<<item<<"dec:"<<as.decrypt(item)<<std::endl;
    }

    

    return 0;
}

