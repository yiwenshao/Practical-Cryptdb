#include "main/big_proxy.hh"
#include <vector>
using std::string;

std::vector<string> create{
"create database tdb;",
"show databases;"
};

int
main(int argc,char ** argv) {
    big_proxy b;
    for(auto item:create){
        b.go(item);
    }
    return 0;
}

