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
static std::string embeddedDir="/t/cryt/shadow";
int main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    init_mysql(embeddedDir);
    std::string query = "insert into student values(NULL)";
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse("tdb", query));
    UNUSED(p);
    return 0;
}
