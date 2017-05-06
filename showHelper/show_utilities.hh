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
using std::string;





namespace show{
    const std::string BOLD_BEGIN = "\033[1m";
    const std::string RED_BEGIN = "\033[1;31m";
    const std::string GREEN_BEGIN = "\033[1;92m";
    const std::string COLOR_END = "\033[0m";
    
    enum color{
        BLANK,
        RED,
        GREEN,
        BOLD
    };

    //output char*
    void output(char *content, color c = BLANK);
    template<class T,class ...arg>
    void output(T t,arg... rest){
        if(t==NULL) {
            std::cout<<"NULL"<<"\t";
        }else{
            std::cout<<std::string(t)<<"\t";
        }
        output(rest...);
    }
    //output std::string
    void draw(std::string,color c = BLANK);
    template<class T,class ...arg>
    void draw(T t,arg... rest){
        if(t.size()==0){
            std::cout<<"0string\t";
        }else{
            std::cout<<t<<"\t";
        }
        draw(rest...);
    }

    namespace keytrans{
        extern std::map<int,std::string> trans;
    }
    namespace commandtrans{
        extern std::map<int,std::string> trans;
    }
    namespace altertrans{
        extern std::map<long long,std::string> trans;
    }
    namespace itemtypetrans{
        extern std::map<int,std::string> trans;
    }

    namespace datatypetrans{
        extern std::map<int,string> trans;
    }
    namespace fieldflagtrans{
        extern std::map<int,string> trans;

    }
}







namespace show{
std::string
empty_if_null(const char *const p);

std::unique_ptr<query_parse> getLex(std::string origQuery);

//const LEX* const lex will not pass,that's why she used RiboldMYSQL::constList_iterator in parser/lex_util.hh
void showKeyList(LEX *lex);

void showCreateFiled(LEX* lex);

void showTableList(LEX *lex);

}



namespace rewrite{
string
lexToQuery(const LEX &lex);


}



