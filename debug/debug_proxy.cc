#include "main/big_proxy.hh"
#include "util/constants.hh"
#include "util/util.hh"
using std::string;
int
main(int argc,char ** argv) {
    big_proxy b("tdb","127.0.0.1","root","letmein",3306);

    std::string filename = std::string(cryptdb_dir)+"/input/"+"sql";
    std::ifstream infile(filename);
    std::string line;
    while(std::getline(infile,line)) {
        if(line.size()>1) {
            b.go(line);
        }
    }
    return 0;
}

