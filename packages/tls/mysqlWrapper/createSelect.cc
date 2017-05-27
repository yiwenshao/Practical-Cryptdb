#include <iostream>
#include "mysqllib/utilities.h"
#include "mysqllib/MyConnect.h"
#include <vector>
#include <string>
using namespace std;
extern Connect *con;
void createSelect(string database,string table){
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
            head+=string("QUOTE(")+fields[i]+"),";
        }
    }
    head[head.size()-1]=' ';
    head += "FROM `"+database+"`.`"+table+"`";
    cout<<head<<endl;
}

int main(int argc,char**argv){
    if(argc==3)return 0;    
    createSelect(string(argv[1]),string(argv[2]));
    return 0;
}

