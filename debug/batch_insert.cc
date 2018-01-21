#include "main/big_proxy.hh"
using std::string;
int
main(int argc,char ** argv) {
    if(argc!=3){
        std::cout<<"1:db, 2:ip"<<std::endl;
        return 0;
    }
    std::string db = std::string(argv[1]);
    std::string ip = std::string(argv[2]);
    big_proxy b(db,ip,"root","letmein",3306);
    std::string initQuery = std::string("use ")+db+";";
    b.go(initQuery);
    std::string query;
    std::getline(std::cin,query);
    long long countWrapper = 0;
    while(query != "quit"){
        if(query.size()==0)
            return 0;
        b.go(query);
        std::getline(std::cin,query);
        countWrapper++;
        if(countWrapper==3){
            std::cout<<"bingo"<<std::endl;
            countWrapper=0;
        }
    }
    return 0;
}

