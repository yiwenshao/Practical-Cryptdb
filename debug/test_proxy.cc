#include "main/big_proxy.hh"
using std::string;
int
main(int argc,char ** argv) {
    if(argc!=2){
        std::cout<<"1:ip"<<std::endl;
        return 0;
    }
    std::string ip = std::string(argv[1]);
    big_proxy b("tdb",ip,"root","letmein",3306);   

    std::string query;
    std::getline(std::cin,query);
    while(query != "quit"){
        b.go(query);
        std::getline(std::cin,query);
    }
    return 0;
}

