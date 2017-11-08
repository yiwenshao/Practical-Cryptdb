#include "main/big_proxy.hh"
#include <vector>
using std::string;
using std::vector;
int
main(int argc,char ** argv) {
    big_proxy b;
    vector<string>queries={
        "show databases;",
        "create database tdb;",
        "use tdb;",
        "create table student(id integer,name varchar(20));",
        "insert into student values(1,'zhang');",
        "select * from student;"
    };
    std::string query;
    for(auto query:queries)
        b.go(query);
    return 0;
}

