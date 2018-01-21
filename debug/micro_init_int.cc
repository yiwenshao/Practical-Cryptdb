#include "main/big_proxy.hh"
#include <vector>
using std::string;

int
main(int argc,char ** argv) {
    std::vector<string> create{
        "create database micro_db;",
        "use micro_db;",
        "create table int_table(id integer);",
    };
    if(argc!=2){
        std::cout<<"1:ip"<<std::endl;
        return 0;
    }
    std::string ip = std::string(argv[1]);
    big_proxy b("tdb",ip,"root","letmein",3306);
    for(auto item:create){
        b.go(item);
    }
    return 0;
}
