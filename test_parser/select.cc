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
#include "parser/showparser_helper.hh"

static std::string embeddedDir="/t/cryt/shadow";
int main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    init_mysql(embeddedDir);
    std::string filename = std::string(cryptdb_dir)+"/test_parser/"+"select";
    std::string line;
    std::ifstream infile(filename);
    while(std::getline(infile,line)) {
        std::cout<<line<<std::endl;
        std::unique_ptr<query_parse> p;
        p = std::unique_ptr<query_parse>(
                new query_parse("tdb", line));
        LEX *const lex = p->lex();
        std::cout<<SHOW::SQLCOM::trans[lex->sql_command]<<std::endl;
        UNUSED(lex);
    }
    return 0;
}
