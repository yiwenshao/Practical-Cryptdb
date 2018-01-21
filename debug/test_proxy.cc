#include "main/big_proxy.hh"
using std::string;
int
main(int argc,char ** argv) {
    big_proxy b;
    std::string query;
    std::getline(std::cin,query);
    while(query != "quit"){
        b.go(query);
        std::getline(std::cin,query);
    }
    return 0;
}

