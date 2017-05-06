#include<iostream>
#include"showHelper/show_DDLHandler.hh"
#include"showHelper/show_Dispatcher.hh"
#include"showHelper/show_utilities.hh"

using std::cout;
using std::endl;

// #### DDLHandler
void showDDLHandler::showAll(const LEX *lex){
    stepOne(lex);
}


// #### CreateTableHandler
// show essential things of create table.
void showCreateTableHandler::stepOne(const LEX *lex){
    std::cout<<"TYPE CREATE_TABLE:"<<std::endl;
    assert(lex!=NULL);
    assert(lex->select_lex.table_list.first);
    if(lex->select_lex.table_list.first){
        show::showTableList(const_cast<LEX*>(lex));
        show::showKeyList(const_cast<LEX*>(lex));
        show::showCreateFiled(const_cast<LEX*>(lex));
    }
}







//## showAlterTableHandler

void showAlterTableHandler::stepOne(const LEX *lex){
    if(alterdis->canDo(lex)){
        const std::vector<showAlterSubHandler*> handlers = alterdis->dispatch(lex);
        assert(handlers.size()>0); 
        for(auto it:handlers){
            it->showAll(lex);
        }      
    }else{

        std::cout<<"unable to recognize the query"<<endl;
    }
}







