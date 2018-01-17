/*
To make this work properly, you should at least make sure that the database tdb exists.
*/
#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>
#include "wrapper/reuse.hh"

static std::string embeddedDir="/t/cryt/shadow";

struct help_select{
    ReturnMeta rmeta;
    std::string query;
};


SharedProxyState *shared_ps;
Connect  *globalConn;
ProxyState *ps;
static
void parseResType(const ResType &rd) {
    for(auto name:rd.names){
        std::cout<<name<<"\t";
    }
    std::cout<<std::endl;
    for(auto row:rd.rows){
        for(auto item:row){
            std::cout<<ItemToString(*item)<<"\t";
        }
            std::cout<<std::endl;
    }
}

/*
static void sp_next_second(const help_select & hs,ResType inRes){
    ps->safeCreateEmbeddedTHD();
    const ResType &resin = inRes;
    try{
        //AbstractQueryExecutor::ResultType::RESULTS
        ps->getSchemaCache().updateStaleness(ps->getEConn(),false);
        const auto &res = decryptResults(resin,hs.rmeta);
        parseResType(res);
    }catch(...){
        std::cout<<"second next error"<<std::endl;
    }
}*/

static void sp_next_first(const help_select &hs){
    ps->safeCreateEmbeddedTHD();
    try{
        //AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN
        ps->getSchemaCache().updateStaleness(ps->getEConn(),false);
        const std::string next_query = hs.query;
        rawMySQLReturnValue resRemote = executeAndGetResultRemote(globalConn,next_query);
        const auto &againGet = MygetResTypeFromLuaTable(false,&resRemote);
        //AbstractQueryExecutor::ResultType::RESULTS
        const auto &res = decryptResults(againGet,hs.rmeta);
        parseResType(res);
    }catch(...){
        std::cout<<"first next error"<<std::endl;
    }
}

//=================================gather part======================================
static 
RewritePlan * 
my_gather(const Item_field &i, Analysis &a){
    const std::string fieldname = i.field_name;
    const std::string table = i.table_name;//we donot deduce here
    FieldMeta &fm =
        a.getFieldMeta(a.getDatabaseName(), table, fieldname);
    const EncSet es = EncSet(a, &fm);
    const std::string why = "is a field";
    reason rsn(es, why, i);
    return new RewritePlan(es, rsn);
}

static
void
my_gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a){
    a.rewritePlans[&i] = std::unique_ptr<RewritePlan>(my_gather(static_cast<const Item_field&>(i), a));
}

static
void
my_process_select_lex(const st_select_lex &select_lex, Analysis &a){
    auto item_it =
        RiboldMYSQL::constList_iterator<Item>(select_lex.item_list);
    for (;;) {
    /*not used in normal insert queries;
      processes id and name in the table student
    */
        const Item *const item = item_it++;
        if (!item)
            break;
        my_gatherAndAddAnalysisRewritePlan(*item, a);
    }
}

static void my_gather_select(Analysis &a, LEX *const lex){
    my_process_select_lex(lex->select_lex, a);
}


//===========================================Rewrite=============================================
static
Item *
    my_do_rewrite_type(const Item_field &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a) {
    const std::string &db_name = a.getDatabaseName();
    const std::string plain_table_name = i.table_name;
    const FieldMeta &fm =
        a.getFieldMeta(db_name, plain_table_name, i.field_name);
    //check if we need onion adjustment
    const OnionMeta &om =
        a.getOnionMeta(db_name, plain_table_name, i.field_name,
                       constr.o);
    const SECLEVEL onion_level = a.getOnionLevel(om);
    assert(onion_level != SECLEVEL::INVALID);

    if (constr.l < onion_level) {
        //need adjustment, throw exception
        const TableMeta &tm =
            a.getTableMeta(db_name, plain_table_name);
        throw OnionAdjustExcept(tm, fm, constr.o, constr.l);
    }
    bool is_alias;
    const std::string anon_table_name =
        a.getAnonTableName(db_name, plain_table_name, &is_alias);
    const std::string anon_field_name = om.getAnonOnionName();
    
    Item_field * const res =
        make_item_field(i, anon_table_name, anon_field_name);

    // HACK: to get aliases to work in DELETE FROM statements
    if (a.inject_alias && is_alias) {
        res->db_name = NULL;
    }
    // This information is only relevant if it comes from a
    // HAVING clause.
    // FIXME: Enforce this semantically.
    a.item_cache[&i] = std::make_pair(res, constr);
    return res;
}


static Item* my_rewrite(const Item &i, const EncSet &req_enc, Analysis &a){
    const std::unique_ptr<RewritePlan> &rp = constGetAssert(a.rewritePlans, &i);

    const EncSet solution = rp->es_out.intersect(req_enc);

    TEST_NoAvailableEncSet(solution, i.type(), req_enc, rp->r.why,
                           std::vector<std::shared_ptr<RewritePlan> >());

    return my_do_rewrite_type(static_cast<const Item_field&>(i), solution.chooseOne(), *rp.get(), a);
}


static void
my_rewrite_proj(const Item &i, const RewritePlan &rp, Analysis &a,
             List<Item> *const newList) {
    AssignOnce<OLK> olk;
    AssignOnce<Item *> ir;

    if (i.type() == Item::Type::FIELD_ITEM) {
        const Item_field &field_i = static_cast<const Item_field &>(i);
        const auto &cached_rewritten_i = a.item_cache.find(&field_i);
        if (cached_rewritten_i != a.item_cache.end()) {
            ir = cached_rewritten_i->second.first;
            olk = cached_rewritten_i->second.second;
        } else {
            ir = my_rewrite(i, rp.es_out, a);
            olk = rp.es_out.chooseOne();
        }
    } else {
        exit(0);
    }
    //和insert不同, select的时候, 只要一个洋葱, 选取一个进行改写就可以了, 不需要扩展.
    assert(ir.assigned() && ir.get());
    newList->push_back(ir.get());
    const bool use_salt = needsSalt(olk.get());

    // This line implicity handles field aliasing for at least some cases.
    // As i->name can/will be the alias.
    addToReturn(&a.rmeta, a.pos++, olk.get(), use_salt, i.name);

    if (use_salt) {
        TEST_TextMessageError(Item::Type::FIELD_ITEM == ir.get()->type(),
            "a projection requires a salt and is not a field; cryptdb"
            " does not currently support such behavior");
        const std::string &anon_table_name =
            static_cast<Item_field *>(ir.get())->table_name;
        const std::string &anon_field_name = olk.get().key->getSaltName();
        Item_field *const ir_field =
            make_item_field(*static_cast<Item_field *>(ir.get()),
                            anon_table_name, anon_field_name);
        newList->push_back(ir_field);
        addSaltToReturn(&a.rmeta, a.pos++);
    }
}



static st_select_lex *
my_rewrite_select_lex(const st_select_lex &select_lex, Analysis &a){
    //do not support filter
    st_select_lex *const new_select_lex = copyWithTHD(&select_lex);
    auto item_it =
        RiboldMYSQL::constList_iterator<Item>(select_lex.item_list);
    List<Item> newList;
    //rewrite item
    for (;;) {
        const Item *const item = item_it++;
        if (!item)
            break;
        my_rewrite_proj(*item,
                     *constGetAssert(a.rewritePlans, item).get(),//get the rewrite plain
                     a, &newList);
    }
    new_select_lex->item_list = newList;
    return new_select_lex;    
}


static
help_select my_rewrite_select(Analysis &a, LEX *lex){
    LEX *const new_lex = copyWithTHD(lex);
    //this is actually table list instead of join list.
    new_lex->select_lex.top_join_list =
            rewrite_table_list(lex->select_lex.top_join_list, a);
    SELECT_LEX *const select_lex_res = my_rewrite_select_lex(new_lex->select_lex, a);
    set_select_lex(new_lex,select_lex_res);
    help_select hs;
    hs.query = lexToQuery(*new_lex);
    hs.rmeta = a.rmeta;
    return hs;
}


static void testCreateTableHandler(std::string query,std::string db="tdb"){
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::unique_ptr<SchemaInfo> schema(new SchemaInfo());

    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }
            return parent;
        };
    //load all metadata and then store it in schema
    loadChildren(schema.get());
   
    const std::unique_ptr<AES_KEY> &TK = 
                         std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    //just like what we do in Rewrite::rewrite,dispatchOnLex
    Analysis analysis(std::string("tdb"),*schema,TK,
                        SECURITY_RATING::SENSITIVE);
    assert(analysis.getMasterKey().get()!=NULL);
    assert(getKey(std::string("113341234"))!=NULL);
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse(db, query));
    LEX *const lex = p->lex();
    std::string table(lex->select_lex.table_list.first->table_name);
    my_gather_select(analysis,lex);
    help_select hs = my_rewrite_select(analysis,lex);
    //QueryRewrite *qr = new QueryRewrite(QueryRewrite(true, analysis.rmeta, analysis.kill_zone, executor));
    sp_next_first(hs);
}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    shared_ps = 
        new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    ps = new ProxyState(*shared_ps);
    globalConn = new Connect(ci.server, ci.user, ci.passwd, ci.port);
    globalConn->execute("use tdb");
    ps->safeCreateEmbeddedTHD();
    std::string query1 = "select * from student;"; 
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testCreateTableHandler(item);
        std::cout<<std::endl;
    }

    return 0;
}
