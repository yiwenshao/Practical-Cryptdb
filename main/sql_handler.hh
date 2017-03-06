#pragma once

#include <main/Analysis.hh>
#include <util/yield.hpp>

#include <sql_lex.h>

#include <map>

#define CR_QUERY_AGAIN(query)                                               \
    std::make_pair(AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN,     \
                   newAnything(std::make_pair(true, std::string(query))))

#define CR_QUERY_RESULTS(query)                                             \
    std::make_pair(AbstractQueryExecutor::ResultType::QUERY_USE_RESULTS,    \
                   newAnything(std::string(query)))

#define CR_RESULTS(value)                                                   \
    std::make_pair(AbstractQueryExecutor::ResultType::RESULTS,              \
                   newAnything(value))

template <typename Type>
class Anything;

class AbstractAnything {
public:
    virtual ~AbstractAnything() = 0;

    template <typename Type>
    Type
    extract() const
    {
        return static_cast<const Anything<Type> *>(this)->get();
    }
};

template <typename Type>
class Anything : public AbstractAnything {
    const Type value;

public:
    Anything(const Type &value) : value(value) {}
    Type get() const {return value;}
};

template <typename Type>
Anything<Type> *
newAnything(const Type &t)
{
    return new Anything<Type>(t);
}

struct NextParams {
    const ProxyState &ps;
    const std::string &default_db;
    const std::string original_query;
    NextParams(const ProxyState &ps, const std::string &default_db,
               const std::string &original_query)
        : ps(ps), default_db(default_db), original_query(original_query) {}
};

class AbstractQueryExecutor {
protected:
    coroutine corot;

public:
    enum class ResultType {RESULTS, QUERY_COME_AGAIN, QUERY_USE_RESULTS};

    AbstractQueryExecutor() {}
    virtual ~AbstractQueryExecutor();
    std::pair<ResultType, AbstractAnything *>
        next(const ResType &res, const NextParams &nparams);

    virtual std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams) = 0;
    virtual bool stales() const {return false;}
    virtual bool usesEmbedded() const {return false;}

private:
    void genericPreamble(const NextParams &nparams);
};

class SimpleExecutor : public AbstractQueryExecutor {
public:
    SimpleExecutor() {}
    ~SimpleExecutor() {}

    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);
};

class NoOpExecutor : public AbstractQueryExecutor {
public:
    NoOpExecutor() {}
    ~NoOpExecutor() {}

    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);
};

class SQLHandler {
public:
    SQLHandler() {;}
    virtual AbstractQueryExecutor * transformLex(Analysis &a, LEX *lex)
        const = 0;
    virtual ~SQLHandler() {;}
};

struct Preamble {
    Preamble(const std::string &dbname, const std::string &table)
        : dbname(dbname), table(table) {}
    const std::string dbname;
    const std::string table;
};

#include <util/unyield.hpp>
