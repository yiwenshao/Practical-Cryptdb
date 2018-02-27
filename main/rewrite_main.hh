#pragma once

/*
 * rewrite_main.hh
 *
 *
 *  TODO: need to integrate it with util.h: some declarations are repeated
 */

#include <exception>
#include <map>

#include <main/Translator.hh>
#include <main/Connect.hh>
#include <main/dispatcher.hh>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>

#include "field.h"

#include <main/Analysis.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
//#include <parser/Annotation.hh>
#include <util/util.hh>
#include <util/onions.hh>

#include <parser/stringify.hh>
#include <parser/lex_util.hh>

#include <util/errstream.hh>
#include <util/cleanup.hh>
#include <util/rob.hh>

extern std::string global_crash_point;

void
crashTest(const std::string &current_point);

class CrashTestException: public std::exception {};

class FieldReturned {
public:
    bool encrypted;
    bool includeInResult;
    std::string key;
    unsigned int SaltIndex;
    std::string nameForReturn;
};

void
printRes(const ResType & r);

//contains the results of a query rewrite:
// - rewritten queries
// - data structure needed to decrypt results
class QueryRewrite {
public:
    QueryRewrite(bool wasRes, ReturnMeta rmeta, const KillZone &kill_zone,
                 AbstractQueryExecutor *const executor)
        : rmeta(rmeta), kill_zone(kill_zone),
          executor(std::unique_ptr<AbstractQueryExecutor>(executor)) {}
    QueryRewrite(QueryRewrite &&other_qr) : rmeta(other_qr.rmeta),
        executor(std::move(other_qr.executor)) {}
    const ReturnMeta rmeta;
    const KillZone kill_zone;
    std::unique_ptr<AbstractQueryExecutor> executor;
};

// Main class processing rewriting
class Rewriter {
    Rewriter();
    ~Rewriter();

public:
    static QueryRewrite
        rewrite(const std::string &q, SchemaInfo const &schema,
                const std::string &default_db,
                const ProxyState &ps);

    static ResType
        decryptResults(const ResType &dbres, const ReturnMeta &rm);

private:
    static AbstractQueryExecutor *
        dispatchOnLex(Analysis &a, const std::string &query);

    static const bool translator_dummy;
    static const std::unique_ptr<SQLDispatcher> dml_dispatcher;
    static const std::unique_ptr<SQLDispatcher> ddl_dispatcher;
};

#define UNIMPLEMENTED                                               \
    FAIL_TextMessageError(std::string("Unimplemented: ") +          \
                            std::string(__PRETTY_FUNCTION__))

class OLK;

class CItemType {
 public:
    virtual RewritePlan *do_gather(const Item &, Analysis &)
        const = 0;
    virtual Item *do_optimize(Item *, Analysis &) const = 0;
    virtual Item *do_rewrite(const Item &, const OLK &constr,
                             const RewritePlan &rp, Analysis &)
        const = 0;
    virtual void do_rewrite_insert(const Item &, const FieldMeta &fm,
                                   Analysis &,
                                   std::vector<Item *> *) const = 0;
};

/*
 * CItemType classes for supported Items: supporting machinery.
 */
//this is cryptdb type. 
template<class T>
class CItemSubtype : public CItemType {
    virtual RewritePlan *do_gather(const Item &i, Analysis &a)
       const
    {
        return do_gather_type(static_cast<const T &>(i), a);
    }

    virtual Item *do_optimize(Item *i, Analysis & a) const
    {
        return do_optimize_type((T*) i, a);
    }

    virtual Item *do_rewrite(const Item &i, const OLK &constr,
                             const RewritePlan &rp,
                             Analysis &a) const
    {
        return do_rewrite_type(static_cast<const T &>(i), constr, rp, a);
    }
    //Rewrite item. If the item is item_field, then rewrite the name and add salt if needed.
    virtual void do_rewrite_insert(const Item &i, const FieldMeta &fm,
                                   Analysis &a,
                                   std::vector<Item *> *l) const
    {
        do_rewrite_insert_type(static_cast<const T &>(i), fm, a, l);
    }

 private:
    virtual RewritePlan *do_gather_type(const T &i, Analysis &a) const
    {
        UNIMPLEMENTED;
    }

    virtual Item *do_optimize_type(T *i, Analysis & a) const
    {
        UNIMPLEMENTED;
        // do_optimize_const_item(i, a);
    }

    virtual Item *do_rewrite_type(const T &i, const OLK &constr,
                                  const RewritePlan &rp,
                                  Analysis &a) const
    {
        UNIMPLEMENTED;
    }

    virtual void do_rewrite_insert_type(const T &i, const FieldMeta &fm,
                                        Analysis &a,
                                        std::vector<Item *> *l) const
    {
        UNIMPLEMENTED;
    }
};



/*
 * Directories for locating an appropriate CItemType for a given Item.
 */
template <class T>
class CItemTypeDir : public CItemType {
 public:
    //add things to types, the map
    void reg(T t, const CItemType &ct)
    {
        auto x = types.find(t);
        if (x != types.end()) {
            thrower() << "duplicate key " << t << std::endl;
        }
        types.insert(std::make_pair(t, &ct));
    }

    RewritePlan *do_gather(const Item &i, Analysis &a) const
    {
        return lookup(i).do_gather(i, a);
    }

    Item* do_optimize(Item *i, Analysis &a) const
    {
        return lookup(*i).do_optimize(i, a);
    }

    Item *do_rewrite(const Item &i, const OLK &constr,
                     const RewritePlan &rp, Analysis &a) const
    {
        return lookup(i).do_rewrite(i, constr, rp, a);
    }

    void do_rewrite_insert(const Item &i, const FieldMeta &fm,
                           Analysis &a, std::vector<Item *> *l) const
    {
        lookup(i).do_rewrite_insert(i, fm, a, l);
    }

protected:
    virtual const CItemType &lookup(const Item &i) const = 0;
    //take a mysql type and return the conresponding cryptdb type.
    const CItemType &do_lookup(const Item &i, const T &t,
                               const std::string &errname) const
    {
        auto x = types.find(t);
        if (x == types.end()) {
            thrower() << "missing " << errname << " " << t << " in "
                      << i << std::endl;
        }
        return *x->second;
    }

 private:
    //map cryptdb type to mysql type
    std::map<T, const CItemType *const > types;
};

class CItemTypesDir : public CItemTypeDir<Item::Type> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, i.type(), "type");
    }
};

extern CItemTypesDir itemTypes;

class CItemFuncDir : public CItemTypeDir<Item_func::Functype> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, static_cast<const Item_func &>(i).functype(),
                         "func type");
    }

public:
    CItemFuncDir()
    {
        itemTypes.reg(Item::Type::FUNC_ITEM, *this);
        itemTypes.reg(Item::Type::COND_ITEM, *this);
    }
};

extern CItemFuncDir funcTypes;

class CItemSumFuncDir : public CItemTypeDir<Item_sum::Sumfunctype> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, static_cast<const Item_sum &>(i).sum_func(),
                         "sumfunc type");
    }

public:
    CItemSumFuncDir()
    {
        itemTypes.reg(Item::Type::SUM_FUNC_ITEM, *this);
    }
};

extern CItemSumFuncDir sumFuncTypes;


class CItemFuncNameDir : public CItemTypeDir<std::string> {
    const CItemType &lookup(const Item &i) const
    {
        return do_lookup(i, static_cast<const Item_func &>(i).func_name(),
                         "func name");
    }

public:
    CItemFuncNameDir() {
        funcTypes.reg(Item_func::Functype::UNKNOWN_FUNC, *this);
        funcTypes.reg(Item_func::Functype::NOW_FUNC, *this);
    }
};

extern CItemFuncNameDir funcNames;


template<class T, Item::Type TYPE>
class CItemSubtypeIT : public CItemSubtype<T> {
public:
    CItemSubtypeIT() {
      itemTypes.reg(TYPE, *this); 
    }
};

template<class T, Item_func::Functype TYPE>
class CItemSubtypeFT : public CItemSubtype<T> {
public:
    CItemSubtypeFT() { 
     funcTypes.reg(TYPE, *this); 
    }
};

template<class T, Item_sum::Sumfunctype TYPE>
class CItemSubtypeST : public CItemSubtype<T> {
public:
    CItemSubtypeST() {
        sumFuncTypes.reg(TYPE, *this); 
    }
};

template<class T, const char *TYPE>
class CItemSubtypeFN : public CItemSubtype<T> {
public:
    CItemSubtypeFN() { 
        funcNames.reg(std::string(TYPE), *this); 
    }
};


std::unique_ptr<SchemaInfo>
loadSchemaInfo(const std::unique_ptr<Connect> &conn,
               const std::unique_ptr<Connect> &e_conn);

class OnionMetaAdjustor {
public:
    OnionMetaAdjustor(OnionMeta const &om) : original_om(om),
        duped_layers(pullCopyLayers(om)) {}
    ~OnionMetaAdjustor() {}

    EncLayer &getBackEncLayer() const;
    EncLayer &popBackEncLayer();
    SECLEVEL getSecLevel() const;
    const OnionMeta &getOnionMeta() const;
    std::string getAnonOnionName() const;

private:
    OnionMeta const &original_om;
    std::vector<EncLayer *> duped_layers;

    static std::vector<EncLayer *> pullCopyLayers(OnionMeta const &om);
};

class OnionAdjustmentExecutor : public AbstractQueryExecutor {
    const std::vector<std::unique_ptr<Delta> > deltas;
    const std::list<std::string> adjust_queries;

    // coroutine state
    bool first_reissue;
    AssignOnce<std::shared_ptr<const SchemaInfo> > reissue_schema;
    AssignOnce<uint64_t> embedded_completion_id;
    AssignOnce<bool> in_trx;
    QueryRewrite *reissue_query_rewrite;
    AssignOnce<NextParams> reissue_nparams;

public:
    OnionAdjustmentExecutor(std::vector<std::unique_ptr<Delta> > &&deltas,
                            const std::list<std::string> &adjust_queries)
        : deltas(std::move(deltas)),
          adjust_queries(adjust_queries), first_reissue(true) {}

    std::pair<ResultType, AbstractAnything *>
        nextImpl(const ResType &res, const NextParams &nparams);

private:
    bool stales() const {return true;}
    bool usesEmbedded() const {return true;}
};






std::pair<std::vector<std::unique_ptr<Delta> >,
                 std::list<std::string>>
adjustOnion(const Analysis &a, onion o, const TableMeta &tm,
            const FieldMeta &fm, SECLEVEL tolevel);

