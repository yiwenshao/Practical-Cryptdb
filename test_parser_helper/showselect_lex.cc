#include <iostream>
#include "test_parser_helper/showselect_lex.hh"
#include "test_parser_helper/showparser.hh"
#include "test_parser_helper/showitem.hh"

void
show_table_list(const List<TABLE_LIST> &tll) {
//alias or nested join
}

void
show_select_lex(const st_select_lex &select_lex) {
    show_table_list(select_lex.top_join_list);
    auto item_it =
        List_iterator<Item>(const_cast<List<Item> &>(select_lex.item_list));
    for(;;) {
        const Item *const item = item_it++;
        if (!item)
            break;
        std::cout<<SHOW::ITEM::trans[item->type()]<<std::endl;
        show_item(item);
    }
}

