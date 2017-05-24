#include <iostream>
#include "mysqllib/utilities.h"
#include "mysqllib/MyConnect.h"
#include <vector>
#include <string>
using namespace std;
extern Connect *con;

//http://php.net/manual/zh/function.mysql-escape-string.php
//https://dev.mysql.com/doc/refman/5.7/en/string-functions.html#function_quote
//backup in configurable extended version
static const int numOfPipe = 3;
void backupselect(){
    string query,table;
    getline(cin,query);
    cin>>table;
    auto dbresult = con->execute(query);
    DBResult * result = dbresult.get();
    vector<vector<string>> rows = result->getRows();
    vector<uint64_t> types = result->getTypes();
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

int main(){
    backupselect(); 
    return 0;
}

