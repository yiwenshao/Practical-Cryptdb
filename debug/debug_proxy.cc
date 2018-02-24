#include "main/big_proxy.hh"
#include "util/constants.hh"
#include "util/util.hh"
using std::string;
int
main(int argc,char ** argv) {
    big_proxy b("tdb","127.0.0.1","root","letmein",3306);

    //std::string query;
    //std::getline(std::cin,query);
    
    std::string filename = std::string(cryptdb_dir)+"/"+"sql";
    std::cout<<filename<<std::endl;
    
    UNUSED(b);

//    while(query != "quit"){
//        b.go(query);
//        std::getline(std::cin,query);
//    }
    return 0;
}

