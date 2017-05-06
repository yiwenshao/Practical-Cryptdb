#pragma once
#include"showHelper/show_SQLHandler.hh"
#include"showHelper/show_Dispatcher.hh"

class showDDLHandler:public showSQLHandler{
public:
    showDDLHandler(){}
    virtual void showAll(const LEX *lex);
    virtual ~showDDLHandler(){}
private:
    virtual void stepOne(const LEX *lex)=0;
};

class showCreateTableHandler:public showDDLHandler{
public:
    showCreateTableHandler(){}    
    virtual ~showCreateTableHandler(){}
private:
    virtual void stepOne(const LEX *lex);
};

class showAlterTableHandler:public showDDLHandler{
public:
    showAlterTableHandler(){}
    virtual ~showAlterTableHandler(){}
private:
    virtual void stepOne(const LEX *lex);
};

