#pragma once

#include <map>

#include <main/Analysis.hh>
#include <main/sql_handler.hh>
#include <main/dispatcher.hh>

#include <sql_lex.h>

class DDLQueryExecutor : public AbstractQueryExecutor {
    AssignOnce<ResType> ddl_res;
    AssignOnce<uint64_t> embedded_completion_id;
public:
    const std::string new_query;
    DDLQueryExecutor(const LEX &new_lex,
                     std::vector<std::unique_ptr<Delta> > &&deltas)
        : new_query(lexToQuery(new_lex)), deltas(std::move(deltas)) {}
    ~DDLQueryExecutor() {}
    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);
private:
    const std::vector<std::unique_ptr<Delta> > deltas;
    bool stales() const {return true;}
    bool usesEmbedded() const {return true;}
};

// Abstract base class for command handler.
class DDLHandler : public SQLHandler {
public:
    virtual AbstractQueryExecutor *transformLex(Analysis &analysis, LEX *lex)
        const;
private:
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *lex, const Preamble &pre) const = 0;
protected:
    DDLHandler() {;}
    virtual ~DDLHandler() {;}
};

class CreateTableHandler : public DDLHandler {
    //must rewrite the header here or we get pure virtual function
    virtual AbstractQueryExecutor *
        rewriteAndUpdate(Analysis &a, LEX *lex, const Preamble &pre) const;
};


SQLDispatcher *buildDDLDispatcher();
