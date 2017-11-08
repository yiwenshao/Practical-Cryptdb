#include "main/big_proxy.hh"
#include <vector>
using std::string;
using std::vector;
int
main(int argc,char ** argv) {
    big_proxy b;
    std::string query;
    std::getline(std::cin,query);
    while(query!="quit"){
        b.go(query);
        std::getline(std::cin,query);
    }
    return 0;
}

