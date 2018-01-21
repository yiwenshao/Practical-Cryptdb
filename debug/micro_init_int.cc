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
    big_proxy b;
    for(auto item:create){
        b.go(item);
    }
    return 0;
}
