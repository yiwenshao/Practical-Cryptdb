#include "big_proxy.hh"
using std::string;


int
main(int argc,char ** argv) {
    big_proxy b;
    b.go("show databases");
    return 0;
}

