#include "test_parser_helper/showstring.hh"

static
std::string
get_select_lex_unit_string( SELECT_LEX_UNIT &select_lex_unit) {
    String s;
    select_lex_unit.print(&s, QT_ORDINARY);
    return std::string(s.ptr(), s.length());
}
std::string
get_lex_string(LEX &lex) {
    String s;
    THD *t = current_thd;
    (void)t;
    switch(lex.sql_command) {
    case SQLCOM_SELECT:
        return get_select_lex_unit_string(lex.unit); 
        break;
    default:
        return "unimplemented";
    }
    return "hehe";
}
