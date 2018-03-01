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
#include "test_parser_helper/showparser.hh"
#include "test_parser_helper/showitem.hh"
#include "test_parser_helper/showselect_lex.hh"
/*
static
void
show_select_lex(const st_select_lex &select_lex) {
    show_table_list(select_lex.top_join_list);
    auto item_it =
        RiboldMYSQL::constList_iterator<Item>(select_lex.item_list);
    for(;;) {
        const Item *const item = item_it++;
        if (!item)
            break;
        UNUSED(item);
        std::cout<<SHOW::ITEM::trans[item->type()]<<std::endl;
        show_item(item);
    }
}*/

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
        std::cout<<"=================================================================="<<std::endl;
        std::cout<<line<<std::endl;
        std::unique_ptr<query_parse> p;
        p = std::unique_ptr<query_parse>(
                new query_parse("tdb", line));
        LEX *const lex = p->lex();
        std::cout<<SHOW::SQLCOM::trans[lex->sql_command]<<std::endl;
        show_select_lex(lex->select_lex);
        UNUSED(lex);
    }
    return 0;
}
