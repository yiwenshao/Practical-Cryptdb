/*
To make this work properly, you should at least make sure that the database tdb exists.
*/
#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>
#include "wrapper/reuse.hh"

using std::vector;
using std::string;

struct batch{
    vector<vector<string>> rows;
    vector<string> fields;
    vector<enum_field_types> types;
    vector<vector<int>> lengths;
};


static
batch executeAndGetBatch(Connect* connect, std::string query){
    MYSQL *conn = connect->get_conn();
    batch bt;
    if (mysql_query(conn,query.c_str())) {
        assert(0);
    }
    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) {
        return bt;
    }
    
    int num_fields = mysql_num_fields(result);
    if(num_fields==0) return bt;

    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> fields;
    std::vector<enum_field_types> types;
    std::vector<std::vector<int>> lengths;

    MYSQL_FIELD *field;
    for(int i=0;i<num_fields;i++){
        field = mysql_fetch_field(result);
        if(field!=NULL){
            fields.push_back(field->name);
            types.push_back(field->type);
        }else{
            std::cout<<"field error"<<std::endl;
        }
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        unsigned long * fieldLen = mysql_fetch_lengths(result);
        vector<string> curRow;
        vector<int> curLength;
        for(int i = 0; i < num_fields; i++) {
            if(row[i]==NULL){
                curRow.push_back(string("NULL"));
                curLength.push_back(0);
            }else{
                curRow.push_back(string(row[i],fieldLen[i]));
                curLength.push_back(fieldLen[i]);
            }
        }
        rows.push_back(curRow);
        lengths.push_back(curLength);
    }
    bt.rows = rows;
    bt.fields = fields;
    bt.lengths = lengths;
    bt.types = types;
    return bt;
}

static
void getTables(Connect*connect, std::string db,std::string table){
    std::string query = std::string("SELECT * FROM ")+table+";";
    batch bt = executeAndGetBatch(connect,query);
    (void)bt;
    return ;
}

int
main(int argc,char** argv) {
    if(argc!=3){
        assert(0);
    }
    std::string db(argv[1]);
    std::string table(argv[2]);

    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    Connect *connect = new Connect(ci.server, ci.user, ci.passwd, ci.port);
    connect->execute(string("use ")+db);
    getTables(connect,db,table);
    return 0;
}
