#pragma once
#include <map>
#include <string>
#include <sql_parse.h>
//#include <mysql.h>
//#include <sql_select.h>
//#include <sql_delete.h>
//#include <sql_insert.h>
//#include <sql_update.h>
//#include <item.h>

void
show_item_field(const Item_field &i);
void
show_item_sum(const Item_sum_sum &i);
void
show_item(const Item *const i);
