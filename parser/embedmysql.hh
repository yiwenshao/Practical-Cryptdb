#pragma once

#include <sstream>
#include <string>
#include <stdexcept>

//#include <parser/Annotation.hh>

#include <util/util.hh>
#include <util/onions.hh>

#include <mysql.h>
#include <sql_base.h>

class query_parse {
 public:
    query_parse(const std::string &db, const std::string &q);
    virtual ~query_parse();
    LEX *lex();
//    Annotation *annot;

 private:
    void cleanup();

    THD *t;
//这里包含了词法分析和语法分析时候, 使用的内部状态.
    Parser_state ps;
};
