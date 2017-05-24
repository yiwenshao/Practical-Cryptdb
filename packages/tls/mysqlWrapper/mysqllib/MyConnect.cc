#include<iostream>
#include<vector>
#include<memory>
#include "MyConnect.h"
#include "utilities.h"
using std::string;
using std::vector;
using std::cout;
using std::endl;

//  http://blog.csdn.net/xiangyu5945/article/details/4808049
Connect *con = new Connect("localhost","root","letmein",3306);

Connect::Connect(const std::string &server, const std::string &user,
                 const std::string &passwd, uint port,bool isMulti)
    : conn(nullptr){
    do_connect(server, user, passwd, port,isMulti);
}

//Connect to MySQL
void
Connect::do_connect(const std::string &server, const std::string &user,
                    const std::string &passwd, uint port,bool isMulti){
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        exit(1);
    }
    unsigned long flag = CLIENT_MULTI_STATEMENTS;
    if(!isMulti) flag = 0;
    if (mysql_real_connect(conn,server.c_str(), user.c_str(), passwd.c_str(),
              0,port,0, flag) == NULL) {
        finish_with_error(conn);
    }
}

void
Connect::finish_with_error(MYSQL *conn,bool close){
    fprintf(stderr, "%s\n", mysql_error(conn));
    if(close)
        mysql_close(conn);
    return;
}


std::string
Connect::getError() {
    return mysql_error(conn);
}

my_ulonglong
Connect::last_insert_id() {
    return mysql_insert_id(conn);
}

unsigned long long
Connect::get_thread_id(){
    if(conn!=NULL)
        return mysql_thread_id(conn);
    else{
        std::cout<<"no connection, no id"<<std::endl;
        return -1;
    }
}

uint64_t
Connect::get_affected_rows() {
    return mysql_affected_rows(conn);
}

void
Connect::get_version(){
    printf("MySQL client version: %s\n", mysql_get_client_info());
}

std::shared_ptr<DBResult>
Connect::execute(const std::string &query){
    if (mysql_query(conn,query.c_str())) {
        finish_with_error(conn,false);
        return std::shared_ptr<DBResult>(NULL);
    }

    MYSQL_RES *result = mysql_store_result(conn);

    if (result == NULL) {
        std::cout<<"no results for this query"<<std::endl;
        return std::shared_ptr<DBResult>(NULL);
    }

    int num_fields = mysql_num_fields(result);

    vector<vector<string>> rows;
    vector<string> fields;
    vector<enum_field_types> types;

    if(num_fields==0){
        return std::make_shared<DBResult>(rows,fields,types);
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        unsigned long * fieldLen = mysql_fetch_lengths(result);
        vector<string> curRow;
        for(int i = 0; i < num_fields; i++) {
            if(row[i]==NULL){
                curRow.push_back(string("NULL"));
            }else{
                curRow.push_back(string(row[i],fieldLen[i]));
            }
        }
        rows.push_back(curRow);
    }

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

    return std::make_shared<DBResult>(rows,fields,types);
}

Connect::~Connect() {
    mysql_close(conn);
}

void
DBResult::printRows(){
    for(auto oneRow:rows){
        for(auto item:oneRow){
            cout<<item<<"\t\t";
        }
        cout<<endl;
    }
}

void DBResult::printRowsF2(){
    for(int i=0;i<(int)fields.size();i++){
        cout<<GREEN_BEGIN<<fields[i]<<":"<<types[i]<<COLOR_END<<endl;
        for(int j=0;j<(int)rows.size();j++){
            cout<<rows[j][i]<<endl;
        }
    }
}


void
DBResult::printFields(){
    for(int i=0;i<(int)fields.size();i++){
        cout<<fields[i]<<"\t\t";
    }
    cout<<endl;
}

vector<vector<string>>
DBResult::getRows(){
    return rows;
}



DBResult::~DBResult(){

}
