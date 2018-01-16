#include "main/big_proxy.hh"
#include <vector>
using std::string;

int
main(int argc,char ** argv) {
    if(argc!=2){
        std::cout<<"expect 1 argument"<<std::endl;
        return 0;
    }
    string length(argv[1]);

    std::vector<string> create{
        "create database micro_db;",
        "use micro_db;",
        "create table int_table(id integer);",
        string("create table str_table(name varchar(")+length+"));"
    };

    big_proxy b;
   
    for(auto item:create){
        b.go(item);
    }
    return 0;
}

