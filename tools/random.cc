#include <string>
#include <iostream>
#include "util/util.hh"
using std::cout;
using std::endl;
int
main(int argc, char* argv[]){
    std::cout<<randomValue()%(0xffffffff)<<std::endl;
    return 0;
}
