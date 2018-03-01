#include <iostream>
#include "test_parser_helper/showselect_lex.hh"
#include "test_parser_helper/showparser.hh"
#include "test_parser_helper/showitem.hh"
static void
show_table_joins_and_derived(const List<TABLE_LIST> &tll) {
    List_iterator<TABLE_LIST> join_it =
                      List_iterator<TABLE_LIST>(const_cast<List<TABLE_LIST> &>(tll));
    while(1) {
        const TABLE_LIST *const t = join_it++;
        if(!t)
            break;
        if(t->nested_join) {
            show_table_joins_and_derived(t->nested_join->join_list);
        }
        if(t->on_expr) {

        }
        if(t->derived) {

        }
    }
}
static void
show_table_aliases(const List<TABLE_LIST> &tll) {
     List_iterator<TABLE_LIST> join_it = 
                       List_iterator<TABLE_LIST>(const_cast<List<TABLE_LIST> &>(tll));
     while(1) {
         const TABLE_LIST *const t = join_it++;
         if(!t)
             break;
         if(t->is_alias) {
             std::cout<<t->db<<","<<t->table_name<<","<<t->alias<<std::endl;             
         }
         if(t->nested_join) {
             show_table_aliases(t->nested_join->join_list);
             return ;
         }
     }
}


//select_lex.top_join_list
void
show_table_list(const List<TABLE_LIST> &tll) {
//alias or nested join
    show_table_aliases(tll);
    show_table_joins_and_derived(tll);
}

void
show_select_lex(const st_select_lex &select_lex) {
    //table list
    show_table_list(select_lex.top_join_list);

    //item list
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

