#include <iostream>
#include <stdlib.h>
#include "mysqllib/utilities.h"
#include "mysqllib/MyConnect.h"
#include <vector>
#include <string>
using namespace std;
extern Connect *con;

string createSelect(string database,string table){
    auto dbresult = con->execute(string("SELECT * FROM `")+database+"`.`"+string(table)+"` LIMIT 1;");
    DBResult * result = dbresult.get();
    vector<vector<string>> rows = result->getRows();
    vector<enum_field_types> types = result->getTypes();
    vector<string> fields = result->getFields();
    string head = "SELECT ";
    for(int i=0;i<types.size();i++){
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

void backupselect(string query,string prefix){
    system((string("rm -rf ")+prefix).c_str());
    system((string("mkdir -p ")+prefix).c_str());
    MYSQL *conn = con->get_conn();
    if (mysql_query(conn,query.c_str())) {
        assert(0);
    }
    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL) {
        return;
    }
    int num_fields = mysql_num_fields(result);
    if(num_fields==0) return;

    vector<vector<string>> rows;
    vector<string> fields;
    vector<enum_field_types> types;
    vector<vector<int>> lengths;
 
    //get fields and types
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

    int len = fields.size();
    vector<FILE*> files(len,NULL);
    for(int i=0;i<fields.size();i++){
        files[i] = fopen((prefix+fields[i]).c_str(),"a");
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
        //rows.push_back(std::move(curRow));
        //lengths.push_back(curLength);
        for(int j=0;j<curRow.size();j++){
            fwrite(curRow[j].c_str(),1,curLength[j],files[j]);
            fprintf(files[j],"\n");
        }
    }
    for(int i=0;i<fields.size();i++){
        fclose(files[i]);
    }
}


vector<string> getTables(string db){
    string query = string("SHOW TABLES IN ")+db;
    auto dbresult = con->execute(query);
    DBResult * result = dbresult.get();
    vector<vector<string>> rows = result->getRows();
    vector<enum_field_types> types = result->getTypes();
    vector<string> fieldNames = result->getFields();
    vector<string> res;
    for(auto item:rows){
        assert(item.size()==1);
        res.push_back(item[0]);
    }
    return res;
}

int main(int argc,char**argv){
    system("rm -rf allTables");
    system("mkdir allTables");
    if(argc!=2){
        cout<<"db"<<endl;
        return 0;
    }
    string num = string(argv[1]);
    vector<string> tablesprefix = getTables(string(argv[1]));
    for(auto item:tablesprefix){
        string query = createSelect(string(argv[1]),item);
        backupselect(query,string("allTables/")+item+"/");
    }
    return 0;
}
