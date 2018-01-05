#include <iostream>
#include "mysqllib/utilities.h"
#include "mysqllib/MyConnect.h"
#include <vector>
#include <string>
using namespace std;
extern Connect *con;

string createSelect(string database,string table,bool isQuote=true){
    auto dbresult = con->execute(string("SELECT * FROM `")+database+"`.`"+string(table)+"` LIMIT 1;");
    DBResult * result = dbresult.get();
    vector<vector<string>> rows = result->getRows();
    vector<enum_field_types> types = result->getTypes();
    vector<string> fields = result->getFields();
    string head = "SELECT ";
    for(int i=0;i<types.size();i++){
        if(IS_NUM(types[i])){
            head += fields[i]+",";
        }
        else{
            if(isQuote)
                head+=string("QUOTE(")+fields[i]+"),";
            else head+=string("HEX(")+fields[i]+"),";
        }
    }
    head[head.size()-1]=' ';
    head += "FROM `"+database+"`.`"+table+"`";
    //cout<<head<<endl;
    return head;
}

//http://php.net/manual/zh/function.mysql-escape-string.php
//https://dev.mysql.com/doc/refman/5.7/en/string-functions.html#function_quote
//backup in configurable extended version
static int numOfPipe = 3;
void backupselect(string query,string table){
    auto dbresult = con->execute(query);
    DBResult * result = dbresult.get();
    vector<vector<string>> rows = result->getRows();
    vector<enum_field_types> types = result->getTypes();
    vector<string> fieldNames = result->getFields();
    string head = string("INSERT INTO ")+"`"+table+"`"+string(" VALUES (");
    for(auto i=0;i<rows.size();i++){
        string cur=head;          
        for(int j=0;j<rows[i].size();j++){
            if(IS_NUM(types[j]))
                cur+=rows[i][j]+",";
            else{
                cur+=rows[i][j]+",";
            }
        }
        cur[cur.size()-1]=')';
        for(int k=1;k<numOfPipe;k++){
            //for each pipe
            i++;
            if(i>=rows.size()) break;
            cur+=",(";
            for(int j=0;j<rows[i].size();j++){
                if(IS_NUM(types[j]))
                    cur+=rows[i][j]+",";
                else{
                    cur+=rows[i][j]+",";
                }
            }
            cur[cur.size()-1]=')';
        }
        cur+=";";
        cout<<cur<<endl;
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
    if(argc!=4){
        cout<<"numOfpipe, db, table"<<endl;
        return 0;
    }
    string num = string(argv[1]);
    numOfPipe = stoi(num);
    string query = createSelect(string(argv[2]),string(argv[3]));

    backupselect(query,string(argv[3])); 
    return 0;
}

