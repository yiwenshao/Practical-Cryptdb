#include "main/big_proxy.hh"
#include <vector>
using std::string;

int
main(int argc,char ** argv) {
    std::vector<string> create{
        "use micro_db;",
        "insert into int_table values(1),(2),(3),(4),(5);",
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
