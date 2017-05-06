#include"showHelper/show_AltersubHandler.hh"
#include"showHelper/show_Dispatcher.hh"
#include"showHelper/show_utilities.hh"


void showAlterSubHandler::showAll(const LEX *lex){
    stepOne(lex);


}


void showAddIndexSubHandler::stepOne(const LEX *lex){
    std::cout<<"add index show"<<std::endl;
    show::showKeyList(lex);    


}


void showForeignKeySubHandler::stepOne(const LEX *lex){
    std::cout<<"add foreign key"<<std::endl;
    show::showKeyList(lex);    

}
