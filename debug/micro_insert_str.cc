#include "main/big_proxy.hh"
#include <vector>
using std::string;

int
main(int argc,char ** argv) {
    if(argc!=2){
        std::cout<<"expect ip"<<std::endl;
        return 0;
    }
    string ip(argv[1]);

    std::vector<string> create{
        "use micro_db;",
        "insert into str_table values('a'),('b'),('c'),('d'),('e');"
    };
    big_proxy b("tdb",ip,"root","letmein",3306);
    for(auto item:create){
        b.go(item);
    }
    return 0;
}

