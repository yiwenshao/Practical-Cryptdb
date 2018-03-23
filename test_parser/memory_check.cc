#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include "parser/sql_utils.hh"
#include "parser/lex_util.hh"
#include "parser/embedmysql.hh"
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
    init_mysql(embeddedDir);//0;0;15,246,176
    std::string line = "show databases;";
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(new query_parse("tdb", line));//0;0;15,246,552
    UNUSED(p);

    return 0;
}
