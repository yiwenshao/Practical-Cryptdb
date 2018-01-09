#include <iostream>
#include <stdlib.h>
#include "mysqllib/utilities.h"
#include "mysqllib/MyConnect.h"
#include <vector>
#include <string>
using namespace std;
extern Connect *con;

void backupSchema(string query){
    auto dbresult = con->execute(query);
    DBResult * result = dbresult.get();
    vector<vector<string>> rows = result->getRows();
    cout<<"/* "<<rows[0][0]<<" */"<<endl;
    cout<<rows[0][1]<<";\n"<<endl;

//    for(auto row:rows){
//        for(int i=0;i<row.size();i++){
//            cout<<row[i]<<endl;
//        }
//    }
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
    string q = string("use ") + db;
    con->execute(q);
    return res;
}

int main(int argc,char**argv){
    if(argc!=2){
        cout<<"db"<<endl;
        return 0;
    }
    string db(argv[1]);
    vector<string> tablesprefix = getTables(db);
    for(auto item:tablesprefix){
        string query = string("show create table ")+item+";";
        backupSchema(query);
    }
    return 0;
}
