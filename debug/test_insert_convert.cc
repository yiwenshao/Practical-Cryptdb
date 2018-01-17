#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>
#include <main/rewrite_main.hh>

static std::string embeddedDir="/t/cryt/shadow";

static void testInsertHandler(std::string query){
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse("tdb", query));
    LEX *const lex = p->lex();
    std::cout<<lexToQuery(*lex)<<std::endl;
}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::string query1("insert into child values(1,\"ZHAA\0\0OY\0\0UN\")",42);
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testInsertHandler(item);
        std::cout<<std::endl;
    }
    return 0;
}
