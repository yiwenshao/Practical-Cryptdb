#include"showHelper/show_entry.hh"
#include"showHelper/show_utilities.hh"

void parseSQL(std::string query){
    show::draw(query,show::RED);
    std::unique_ptr<query_parse> p = show::getLex(query);
    LEX *const lex = p->lex();
    if(ddldis->canDo(lex)){
        std::cout<<"ddl can do"<<std::endl;
        const std::shared_ptr<showSQLHandler> h = ddldis->dispatch(lex);
        h->showAll(lex);
    }else{


    }


}
