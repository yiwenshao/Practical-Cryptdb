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
    string query;
    getline(cin,query);
    while(query!=string("quit")){
        auto dbresult = con->execute(query);
        DBResult * result = dbresult.get();
        if(result!=NULL){
            vector<vector<string>> rows = result->getRows();
            vector<enum_field_types> types = result->getTypes();
            vector<string> fieldNames = result->getFields();
            result->printRows();
        }
        getline(cin,query);
    }

    return 0;
}

