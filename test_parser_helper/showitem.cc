#include <iostream>//this can not be placed below showitem.hh, why?
#include "test_parser_helper/showitem.hh"

void
show_item_field(const Item_field &i) {
    std::cout<<"i.field_name: "<<i.field_name<<std::endl;
}


void
show_item_sum(const Item_sum_sum &i) {
       const unsigned int arg_count = const_cast<Item_sum_sum &>(i).get_arg_count();
       std::cout<<arg_count<<std::endl;
       const Item *const child_item = const_cast<Item_sum_sum &>(i).get_arg(0);
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


