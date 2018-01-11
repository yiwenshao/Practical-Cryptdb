#include "main/big_proxy.hh"
#include <vector>
using std::string;

std::vector<string> create{
"create database tdbxxx;",
"use tdbxxx;",
"create table student(id integer,name varchar(20));"
};


static 
std::vector<std::vector<std::string>> 
get_rows(Connect* conn,std::string query){
    std::unique_ptr<DBResult> dbres;
    conn->execute(query,&dbres);
    const size_t row_count = static_cast<size_t>(mysql_num_rows(dbres->n));
    const int col_count    = mysql_num_fields(dbres->n);
    std::vector<std::vector<std::string> > rows;
    for (size_t index = 0;; index++) {
        const MYSQL_ROW row = mysql_fetch_row(dbres->n);
        if (!row) {
            assert(row_count == index);
            break;
        }
        unsigned long *const lengths = mysql_fetch_lengths(dbres->n);
        std::vector<std::string> resrow;
        for (int j = 0; j < col_count; j++) {
            std::string item(row[j],lengths[j]);
            resrow.push_back(item);
        }
        rows.push_back(resrow);
    }
    return rows;
}

static
void get_and_show(Connect* conn,std::string query){
    std::unique_ptr<DBResult> dbres;
    conn->execute(query,&dbres);
    dbres->showResults();
}


int
main(int argc,char ** argv) {
    big_proxy b;
    for(auto item:create){
        b.go(item);
    }
    Connect  *tc = new Connect("localhost","root","letmein",3306);
    get_and_show(tc,"use tdbxxx");
    auto res = get_rows(tc,"show tables");
    std::string query="show create table ";
    query+=res[0][0]+=";";
    get_and_show(tc,query);
    b.go("drop database tdbxxx;");
    return 0;
}

