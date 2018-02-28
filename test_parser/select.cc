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
#include "parser/lex_util.hh"

static void show_item(const Item* const i);

static
void
show_item_field(const Item_field &i) {
    std::cout<<"i.field_name: "<<i.field_name<<std::endl;

}


static
void
show_item_sum(const Item_sum_sum &i) {
     const unsigned int arg_count = RiboldMYSQL::get_arg_count(i);
     std::cout<<arg_count<<std::endl;
     const Item *const child_item = RiboldMYSQL::get_arg(i, 0);
     show_item(child_item);
}

void
show_item(const Item * const i) {
    switch (i->type()){
    case Item::FIELD_ITEM: {
        show_item_field(static_cast<const Item_field&>(*i));
        break;
    }
    case Item::SUM_FUNC_ITEM: {
        show_item_sum(static_cast<const Item_sum_sum &>(*i));
        break;
    }
    case Item::INT_ITEM: {

        break;
    }
    case Item::STRING_ITEM:{

        break;
    }
    default:{
        std::cout<<"unknown type"<<std::endl;
    }
    }
}



static
void
show_table_list(const List<TABLE_LIST> &tll) {
//alias or nested join


}


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
}

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
