#include"showHelper/show_Dispatcher.hh"

bool showSQLDispatcher::canDo(LEX *const lex)const{
    auto command = extract(lex);
    return handlers.find(command)!=handlers.end();
}
long long showSQLDispatcher::extract(LEX *const lex)const{
    return lex->sql_command;

}

const std::shared_ptr<showSQLHandler> showSQLDispatcher::dispatch(LEX *const lex) const{
    auto it = handlers.find(extract(lex));
    assert(it!=handlers.end());
    assert(it->second);
    return it->second;
}




///showAlterDispatcher
bool showAlterDispatcher::canDo(LEX *const lex)const{
    if (0 == lex->alter_info.flags) {
        return false;
    }

    long long flags = lex->alter_info.flags;
    for (const auto &it : handlers) {
        flags -= lex->alter_info.flags & it.first;
    }
    return 0 == flags;
}


//get handlers from lex->alter_info.flags
std::vector<showAlterSubHandler *> showAlterDispatcher::dispatch(LEX *const lex) const{
    std::vector<showAlterSubHandler*> out;
    for (const auto &it : handlers) {
        const long long extract = lex->alter_info.flags & it.first;
        if (extract) {
            auto it_handler = handlers.find(extract);
            assert(handlers.end() != it_handler && it_handler->second);
            out.push_back(it_handler->second.get());
        }
    }
    return out;
}



/*
able to support
    SQLCOM_CREATE_TABLE,

*/
showSQLDispatcher* buildShowDDLDispatcher(){
    showDDLHandler *h;
    showSQLDispatcher * dispatcher = new showSQLDispatcher;
    h = new showCreateTableHandler;
    dispatcher->addHandler(SQLCOM_CREATE_TABLE, h);
    h = new showAlterTableHandler;
    dispatcher->addHandler(SQLCOM_ALTER_TABLE,h);

    return dispatcher;
}



showAlterDispatcher* buildShowAlterDispatcher(){
    showAlterSubHandler *h;
    showAlterDispatcher *dispatcher = new showAlterDispatcher;
    h = new showAddIndexSubHandler;
    dispatcher->addHandler(ALTER_ADD_INDEX,h);
    h = new showForeignKeySubHandler;
    dispatcher->addHandler(ALTER_FOREIGN_KEY,h);
    return dispatcher;
}



extern const showSQLDispatcher* const ddldis = buildShowDDLDispatcher();
extern const showAlterDispatcher* const alterdis = buildShowAlterDispatcher();




