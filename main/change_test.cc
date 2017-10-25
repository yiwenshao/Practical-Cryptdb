#include "big_proxy.hh"
#include <vector>
using std::string;
using std::vector;

vector<string> queries{
   "show databases;",
   "create database if not exists tdb2;",
   "use tdb2;",
   "create table if not exists student(id integer, name varchar(50));",
   "insert into student values(1,'shao'),(2,'xiaocai'),(3,'nans'),(4,'hehe'),(5,'oo');",
   "select * from student;",
   "select id from student;",
   "select name from student;",
   "select sum(id) from student;"
};

int
main(int argc,char ** argv) {
    big_proxy b;
    for(auto query:queries){
        b.go(query);
        std::cout<<query<<"   pass!"<<std::endl;
    }
    std::cout<<"pass ALL !!"<<std::endl;
    return 0;
}
