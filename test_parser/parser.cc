#include <string>
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
#include "util/util.hh"
#include "util/constants.hh"

static std::string embeddedDir="/t/cryt/shadow";
int main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    free(buffer);
    init_mysql(embeddedDir);
    std::string filename = std::string(cryptdb_dir)+"/test_parser/"+"template";
    std::string line="show databases;";
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(new query_parse("tdb", line));
    LEX *const lex = p->lex();
    UNUSED(lex);
    return 0;
}
