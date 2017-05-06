#pragma once

#include"showHelper/show_SQLHandler.hh"
#include"showHelper/show_DDLHandler.hh"
#include"showHelper/show_AltersubHandler.hh"

template <typename FetchMe>
class showDispatcher {
public:
    virtual ~showDispatcher() {}
    bool addHandler(long long cmd, FetchMe *const h) {
        if (NULL == h) {
            return false;
        }   
        auto it = handlers.find(cmd);
        if (handlers.end() != it) {
            return false;
        }    
        handlers[cmd] = std::unique_ptr<FetchMe>(h);
        return true;
    }   
    virtual bool canDo(LEX *const lex) const = 0;
protected:
    std::map<long long, std::shared_ptr<FetchMe>> handlers;
};

class showSQLDispatcher : public showDispatcher<showSQLHandler> {
public:
    showSQLDispatcher(){}
    bool canDo(LEX *const lex) const;
    const std::shared_ptr<showSQLHandler> dispatch(LEX *const lex) const;    
private:
    virtual long long extract(LEX *const lex) const;
};  
    

class showAlterDispatcher : public showDispatcher<showAlterSubHandler> {
public:
    std::vector<showAlterSubHandler *> dispatch(LEX *const lex) const;
    bool canDo(LEX *const lex) const;
};



showSQLDispatcher* buildShowDDLDispatcher();
showAlterDispatcher* buildShowAlterDispatcher();

extern const showSQLDispatcher* const ddldis;
extern const showAlterDispatcher* const alterdis;

