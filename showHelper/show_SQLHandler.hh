#pragma once

#include <util/util.hh>

#include <string>

#include <sql_parse.h>

#include <parser/mysql_type_metadata.hh>
#include <parser/sql_utils.hh>

#include <crypto/BasicCrypto.hh>

#include <assert.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

#include <parser/embedmysql.hh>
#include <sql_base.h>
#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>
#include <sql_parse.h>
#include <handler.h>


#include <parser/stringify.hh>

#include <util/errstream.hh>
#include <util/rob.hh>
#include <iostream>


class showSQLHandler{
public:
    showSQLHandler(){}
    virtual void showAll(const LEX *lex)=0;
    virtual ~showSQLHandler(){}
};


class showAlterSubHandler{
public:
    showAlterSubHandler(){}
    virtual void showAll(const LEX *lex);
    virtual ~showAlterSubHandler(){}
private:
    virtual void stepOne(const LEX *lex)=0;
};


