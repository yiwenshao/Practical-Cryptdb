#pragma once
#include"showHelper/show_SQLHandler.hh"

class showAddIndexSubHandler:public showAlterSubHandler{
public:
    showAddIndexSubHandler(){}
    virtual ~showAddIndexSubHandler(){}
private:
    virtual void stepOne(const LEX *lex);
};

class showForeignKeySubHandler:public showAlterSubHandler{
public:
    showForeignKeySubHandler(){}
    virtual ~showForeignKeySubHandler(){}
private:
    virtual void stepOne(const LEX *lex);
};

