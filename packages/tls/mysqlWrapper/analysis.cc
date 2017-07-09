#include <iostream>
#include <stdlib.h>
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
}

int main(int argc,char**argv){
    system("rm -rf allTables");
    system("mkdir allTables");
    if(argc!=3){
        cout<<"db, table"<<endl;
        return 0;
    }
    string db(argv[1]),table(argv[2]);

    createSelect(db,table);
    return 0;
}
