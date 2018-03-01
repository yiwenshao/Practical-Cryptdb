#pragma once
//#include <sql_select.h>
//#include <sql_delete.h>
//#include <sql_insert.h>                                        
//#include <sql_update.h>
#include <map>
#include <string>
#include <sql_parse.h>
#include <mysql.h>


namespace SHOW{
    namespace KEY{
        extern std::map<int,std::string> trans;
    }
    namespace SQLCOM{
        extern std::map<int,std::string> trans;
    }
    namespace ALTER{
        extern std::map<long long,std::string> trans;
    }
    namespace ITEM{
        extern std::map<int,std::string> trans;
    }
}
