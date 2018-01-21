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

static std::string embeddedDir="/t/cryt/shadow";
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
string createSelect(Connect*connect,string database,string table){
    auto res = executeAndGetBatch(connect, string("SELECT * FROM `")+database+"`.`"+string(table)+"` LIMIT 1;");
    vector<vector<string>> rows = res.rows;
    vector<enum_field_types> types = res.types;
    vector<string> fields = res.fields;
    string head = "SELECT ";
    for(unsigned int i=0u;i<types.size();i++){
        if(IS_NUM(types[i])){
            head += fields[i]+",";
        }else{
            head += fields[i]+",";
        }
    }
    head[head.size()-1]=' ';
    head += "FROM `"+database+"`.`"+table+"`";
    return head;
}

static
void backupselect(Connect*connect, string query,string prefix){
    system((string("rm -rf ")+prefix).c_str());
    system((string("mkdir -p ")+prefix).c_str());

    MYSQL *conn = connect->get_conn();
    if (mysql_query(conn,query.c_str())) {
        assert(0);
    }
    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL) {
        return;
    }
    unsigned int num_fields = mysql_num_fields(result);
    if(num_fields==0) return;

    vector<vector<string>> rows;
    vector<string> fields;
    vector<enum_field_types> types;
    vector<vector<int>> lengths;

    //get fields and types
    MYSQL_FIELD *field;
    for(unsigned int i=0u;i<num_fields;i++){
        field = mysql_fetch_field(result);
        if(field!=NULL){
            fields.push_back(field->name);
            types.push_back(field->type);
        }else{
            std::cout<<"field error"<<std::endl;
        }
    }
    unsigned int len = fields.size();
    vector<FILE*> files(len,NULL);
    for(unsigned int i=0u;i<fields.size();i++){
        files[i] = fopen((prefix+fields[i]).c_str(),"a");
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        unsigned long * fieldLen = mysql_fetch_lengths(result);
        vector<string> curRow;
        vector<int> curLength;
        for(unsigned int i = 0u; i < num_fields; i++) {
            if(row[i]==NULL){
                curRow.push_back(string("NULL"));
                curLength.push_back(0);
            }else{
                curRow.push_back(string(row[i],fieldLen[i]));
                curLength.push_back(fieldLen[i]);
            }
        }
        //rows.push_back(std::move(curRow));
        //lengths.push_back(curLength);
        for(unsigned int j=0u;j<curRow.size();j++){
            fwrite(curRow[j].c_str(),1,curLength[j],files[j]);
            if(IS_NUM(types[j]))
                fprintf(files[j],"\n");
        }
    }
    for(unsigned int i=0u;i<fields.size();i++){
        fclose(files[i]);
    }
}


static
std::vector<std::string> getTables(Connect*connect, std::string db){
    std::string query = std::string("SHOW TABLES IN ")+db;

    batch bt = executeAndGetBatch(connect,query);
    
    std::vector<std::vector<std::string>> rows = bt.rows;
    std::vector<enum_field_types> types = bt.types;
    std::vector<std::string> fieldNames = bt.fields;
    std::vector<std::string> res;
    for(auto item:rows){
        assert(item.size()==1);
        res.push_back(item[0]);
    }
    return res;
}

int
main(int argc,char** argv) {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    if(argc!=2){
        assert(0);
    }    
    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    SharedProxyState *shared_ps = 
        new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::string db = std::string(argv[1]);;

    Connect *connect = new Connect(ci.server, ci.user, ci.passwd, ci.port);
    connect->execute(string("use ")+db);
    auto tablesPrefix = getTables(connect,db);    
    for(auto item:tablesPrefix){
        string query = createSelect(connect,db,item);
        backupselect(connect,query,string("allTables/")+item+"/");
    }
    return 0;
}
