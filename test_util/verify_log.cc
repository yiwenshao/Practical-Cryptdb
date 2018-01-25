#include "util/log.hh"
#include "util/constants.hh"
#include <string>
#include <iostream>
using std::cout;
using std::endl;

int main(){
    std::string line("abcdefghehe");
    logToFile log(constGlobalConstants.logFile);
    log<<line<<"\n";
    return 0;
}
