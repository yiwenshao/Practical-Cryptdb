#ifndef MYCONNECT_H_INCLUDED
#define MYCONNECT_H_INCLUDED

#include "utilities.h"
#include <vector>
#include <my_global.h>
#include <mysql.h>
#include <memory>
#include <map>

using std::string;
using std::vector;
using std::map;



//only static allowed
static map<int, string> gtm={
    {MYSQL_TYPE_DECIMAL,"MYSQL_TYPE_DECIMAL"},
    {MYSQL_TYPE_TINY,"MYSQL_TYPE_TINY"},
    {MYSQL_TYPE_SHORT,"MYSQL_TYPE_SHORT"},
    {MYSQL_TYPE_LONG,"MYSQL_TYPE_LONG"},
    {MYSQL_TYPE_FLOAT,"MYSQL_TYPE_FLOAT"},
    {MYSQL_TYPE_DOUBLE,"MYSQL_TYPE_DOUBLE"},
    {MYSQL_TYPE_NULL,"MYSQL_TYPE_NULL"},
    {MYSQL_TYPE_TIMESTAMP,"MYSQL_TYPE_TIMESTAMP"},
    {MYSQL_TYPE_LONGLONG,"MYSQL_TYPE_LONGLONG"},
    {MYSQL_TYPE_INT24,"MYSQL_TYPE_INT24"},
    {MYSQL_TYPE_DATE,"MYSQL_TYPE_DATE"},
    {MYSQL_TYPE_TIME,"MYSQL_TYPE_TIME"},
    {MYSQL_TYPE_DATETIME,"MYSQL_TYPE_DATETIME"},
    {MYSQL_TYPE_YEAR,"MYSQL_TYPE_YEAR"},
    {MYSQL_TYPE_NEWDATE,"MYSQL_TYPE_NEWDATE"},
    {MYSQL_TYPE_VARCHAR,"MYSQL_TYPE_VARCHAR"},
    {MYSQL_TYPE_BIT,"MYSQL_TYPE_BIT"},
    {MYSQL_TYPE_NEWDECIMAL,"MYSQL_TYPE_NEWDECIMAL"},
    {MYSQL_TYPE_ENUM,"MYSQL_TYPE_ENUM"},
    {MYSQL_TYPE_SET,"MYSQL_TYPE_SET"},
    {MYSQL_TYPE_TINY_BLOB,"MYSQL_TYPE_TINY_BLOB"},
    {MYSQL_TYPE_MEDIUM_BLOB,"MYSQL_TYPE_MEDIUM_BLOB"},
    {MYSQL_TYPE_LONG_BLOB,"MYSQL_TYPE_LONG_BLOB"},
    {MYSQL_TYPE_BLOB,"MYSQL_TYPE_BLOB"},
    {MYSQL_TYPE_VAR_STRING,"MYSQL_TYPE_VAR_STRING"},
    {MYSQL_TYPE_STRING,"MYSQL_TYPE_STRING"},
    {MYSQL_TYPE_GEOMETRY,"MYSQL_TYPE_GEOMETRY"}
};

class DBResult {
 public:
    DBResult():affected_rows(-1),insert_id(-1){}
    DBResult(vector<vector<string>> inRows,vector<string >inFields,
                    vector<enum_field_types> inTypes):affected_rows(-1),insert_id(-1),
            rows(inRows),fields(inFields),types(inTypes){
            for(auto item:types){
                typesString.push_back(gtm[item]);
            }
    }
    void printRowsF2();
    void printRows();
    void printFields();
    vector<vector<string>> getRows();
    vector<enum_field_types> getTypes(){return types;}
    vector<string> getTypesString(){return typesString;}
    vector<string> getFields(){return fields;}
    ~DBResult();
 private:
    const uint64_t affected_rows;
    const uint64_t insert_id;
    const vector<vector<string>> rows;
    const vector<string> fields;
    const vector<enum_field_types> types;
    vector<string> typesString;
};

class Connect {
 public:
    Connect(const std::string &server, const std::string &user,
            const std::string &passwd, uint port = 0,bool isMulti=true);
    // returns true if execution was ok; caller must delete DBResult
    std::shared_ptr<DBResult> execute(const std::string &query);
    // returns error message if a query caused error
    std::string getError();
    my_ulonglong last_insert_id();
    unsigned long long get_thread_id();
    unsigned int get_mysql_errno();
    uint64_t get_affected_rows();
    void get_version();
    void finish_with_error(MYSQL *con,bool close = true);
    ~Connect();
 private:
    MYSQL *conn;
    void do_connect(const std::string &server, const std::string &user,
                    const std::string &passwd, uint port,bool isMulti);
};


extern Connect *con;

#endif // MYCONNECT_H_INCLUDED
