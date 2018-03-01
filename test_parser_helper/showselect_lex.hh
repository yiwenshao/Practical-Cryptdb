#pragma once
#include <map>
#include <string>
#include <sql_parse.h>

void
show_table_list(const List<TABLE_LIST> &tll);

void
show_select_lex(const st_select_lex &select_lex);

