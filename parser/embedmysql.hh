#pragma once

#include <sstream>
#include <string>
#include <stdexcept>
#include <util/util.hh>
#include <util/onions.hh>
#include <mysql.h>
#include <sql_base.h>

class query_parse {
 public:
    query_parse(const std::string &db, const std::string &q);
    virtual ~query_parse();
    LEX *lex();
 private:
    void cleanup();
    THD *t; /*used to hold current_thd*/
    Parser_state ps; /*the internal state used by parser*/
};

