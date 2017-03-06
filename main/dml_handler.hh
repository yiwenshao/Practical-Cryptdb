#pragma once

#include <map>

#include <main/Analysis.hh>
#include <main/sql_handler.hh>
#include <main/dispatcher.hh>

#include <sql_lex.h>

class DMLQueryExecutor : public AbstractQueryExecutor {
public:
    DMLQueryExecutor(const LEX &lex, const ReturnMeta &rmeta)
        : query(lexToQuery(lex)), rmeta(rmeta) {}
    ~DMLQueryExecutor() {}
    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);

private:
    const std::string query;
    const ReturnMeta rmeta;
};

class SpecialUpdateExecutor : public AbstractQueryExecutor {
    const std::string original_query;
    const std::string plain_table;
    const std::string crypted_table;
    const std::string where_clause;

    // coroutine state
    AssignOnce<ResType> dec_res;
    AssignOnce<DBResult *> original_query_dbres;
    AssignOnce<std::string> escaped_output_values;
    AssignOnce<ReturnMeta> select_rmeta;
    AssignOnce<bool> in_trx;

public:
    SpecialUpdateExecutor(const std::string &plain_table,
                          const std::string &crypted_table,
                          const std::string &where_clause)
        : plain_table(plain_table), crypted_table(crypted_table),
          where_clause(where_clause) {}
    ~SpecialUpdateExecutor() {}
    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);

private:
    bool usesEmbedded() const {return true;}
};

class ShowDirectiveExecutor : public AbstractQueryExecutor {
    const SchemaInfo &schema;

public:
    ShowDirectiveExecutor(const SchemaInfo &schema)
        : schema(schema) {}
    ~ShowDirectiveExecutor() {}

    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);

private:
    bool usesEmbedded() const {return true;}

    static bool
    deleteAllShowDirectiveEntries(const std::unique_ptr<Connect> &e_conn);

    static bool
    addShowDirectiveEntry(const std::unique_ptr<Connect> &e_conn,
                          const std::string &database,
                          const std::string &table,
                          const std::string &field,
                          const std::string &onion,
                          const std::string &level);

    static bool
    getAllShowDirectiveEntries(const std::unique_ptr<Connect> &e_conn,
                               std::unique_ptr<DBResult> *db_res);
};

class SensitiveDirectiveExecutor : public AbstractQueryExecutor {
    const std::vector<std::unique_ptr<Delta> > deltas;

public:
    SensitiveDirectiveExecutor(std::vector<std::unique_ptr<Delta> > &&deltas)
        : deltas(std::move(deltas)) {}
    ~SensitiveDirectiveExecutor() {}

    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);

private:
    bool stales() const {return true;}
    bool usesEmbedded() const {return true;}
};

class ShowTablesExecutor : public AbstractQueryExecutor {
    const std::vector<std::unique_ptr<Delta> > deltas;

public:
    ShowTablesExecutor() {}
    ~ShowTablesExecutor() {}

    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);
};

// Abstract base class for query handler.
class DMLHandler : public SQLHandler {
public:
    virtual AbstractQueryExecutor *
        transformLex(Analysis &a, LEX *lex) const;

private:
    virtual void gather(Analysis &a, LEX *lex) const = 0;
    virtual AbstractQueryExecutor * rewrite(Analysis &a, LEX *lex) const = 0;

protected:
    DMLHandler() {;}
    virtual ~DMLHandler() {;}
};

SQLDispatcher *buildDMLDispatcher();

