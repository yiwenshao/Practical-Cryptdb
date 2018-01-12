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

static std::string embeddedDir="/t/cryt/shadow";

SharedProxyState *shared_ps;
Connect  *globalConn;
ProxyState *ps;

struct rawReturnValue{
    std::vector<std::vector<std::string> > rowValues;
    std::vector<std::string> fieldNames;
    std::vector<int> fieldTypes;
};

//helper function for transforming the rawReturnValue
static Item_null *
make_null(const std::string &name = ""){
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}
//helper function for transforming the rawReturnValue
static std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null());
    }
    return out;
}



static
ResType MygetResTypeFromLuaTable(bool isNULL,rawReturnValue *inRow = NULL,int in_last_insert_id = 0){
    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    std::vector<std::vector<Item *> > rows;
    //return NULL restype 
    if(isNULL){
        return ResType(true,0,0,std::move(names),
                      std::move(types),std::move(rows));
    } else {
        for(auto inNames:inRow->fieldNames){
            names.push_back(inNames);
        }
        for(auto inTypes:inRow->fieldTypes){
            types.push_back(static_cast<enum_field_types>(inTypes));
        }
        for(auto inRows:inRow->rowValues) {
            std::vector<Item *> curTempRow = itemNullVector(types.size());
            for(int i=0;i< (int)(inRows.size());i++){
                curTempRow[i] = (MySQLFieldTypeToItem(types[i],inRows[i]) );
            }
            rows.push_back(curTempRow);
        }
        return ResType(true,0,in_last_insert_id, 
                       std::move(names),
                       std::move(types), std::move(rows));
    }
}

static
rawReturnValue executeAndGetResultRemote(Connect * curConn,std::string query){
    std::unique_ptr<DBResult> dbres;
    curConn->execute(query, &dbres);
    rawReturnValue myRaw;

    if(dbres==nullptr||dbres->n==NULL){
        //std::cout<<"no results"<<std::endl;
        return myRaw;
    }

    int num = mysql_num_rows(dbres->n);
    if(num!=0)
        std::cout<<"num of rows: "<<num<<std::endl;

    int numOfFields = mysql_num_fields(dbres->n);
    if(numOfFields!=0)
        std::cout<<"num of fields: "<<numOfFields<<std::endl;

    MYSQL_FIELD *field;
    MYSQL_ROW row;

    if(num!=0){
        while( (row = mysql_fetch_row(dbres->n)) ){
            unsigned long * fieldLen = mysql_fetch_lengths(dbres->n);
            std::vector<std::string> curRow;
            for(int i=0;i<numOfFields;i++){
                if (i == 0) {
                    while( (field = mysql_fetch_field(dbres->n)) ) {
                        myRaw.fieldNames.push_back(std::string(field->name));
                        myRaw.fieldTypes.push_back(field->type);
                    }
                }
                if(row[i]==NULL) curRow.push_back("NULL");
                else curRow.push_back(std::string(row[i],fieldLen[i]));
            }
            myRaw.rowValues.push_back(curRow);
        }
    }
    return myRaw;
}

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

static void sp_next(std::string db, std::string query, QueryRewrite *qr,ResType inRes){

    ps->safeCreateEmbeddedTHD();
    //then we come to the next step.
    const ResType &res = inRes;//MygetResTypeFromLuaTable(true);
    try{
        NextParams nparams(*ps,db,query);
        const auto &new_results = qr->executor->next(res, nparams);
        const auto &result_type = new_results.first;
        switch (result_type){
            //execute the query, fetch the results, and call next again
            case AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN: {
            //    std::cout<<RED_BEGIN<<"case one"<<COLOR_END<<std::endl;
                const auto &output =
                    std::get<1>(new_results)->extract<std::pair<bool, std::string> >();
                const auto &next_query = output.second;
                //here we execute the query against the remote database, and get rawReturnValue
                rawReturnValue resRemote = executeAndGetResultRemote(globalConn,next_query);
                //transform rawReturnValue first
                const auto &againGet = MygetResTypeFromLuaTable(false,&resRemote);
                
                sp_next(db,query,qr,againGet);
                break;
            }
    
            //only execute the query, without processing the retults
            case AbstractQueryExecutor::ResultType::QUERY_USE_RESULTS:{
            //    std::cout<<RED_BEGIN<<"case two"<<COLOR_END<<std::endl;
                const auto &new_query =
                    std::get<1>(new_results)->extract<std::string>();
                auto resRemote = executeAndGetResultRemote(globalConn,new_query);
               
                break;
            }
    
            //return the results to the client directly 
            case AbstractQueryExecutor::ResultType::RESULTS:{
            //    std::cout<<RED_BEGIN<<"case three"<<COLOR_END<<std::endl;
                const auto &res = new_results.second->extract<ResType>();
                parseResType(res);              
                break;
            }
    
            default:{
                std::cout<<"case default"<<std::endl;
            }
        }
    }catch(...){
        std::cout<<"next error"<<std::endl;
    }
}

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
my_gatherAndAddAnalysisRewritePlan(const Item &i, Analysis &a)
{
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


//==========================================================Rewrite=================================================


static void
my_addToReturn(ReturnMeta *const rm, int pos, const OLK &constr,
            bool has_salt, const std::string &name){
    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();
    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");

    const int salt_pos = has_salt ? pos + 1 : -1;
    std::pair<int, ReturnField>
        pair(pos, ReturnField(false, name, constr, salt_pos));
    rm->rfmeta.insert(pair);
}

static void
my_addSaltToReturn(ReturnMeta *const rm, int pos)
{
    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();
    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");

    std::pair<int, ReturnField>
        pair(pos, ReturnField(true, "", OLK::invalidOLK(), -1));
    rm->rfmeta.insert(pair);
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
            //对于select中的选择域来说,这里对应的是rewrite_field.cc中的83, do_rewrite_type
            ir = rewrite(i, rp.es_out, a);
            olk = rp.es_out.chooseOne();
        }
    } else {
        ir = rewrite(i, rp.es_out, a);
        olk = rp.es_out.chooseOne();
    }
    //和insert不同, select的时候, 只要一个洋葱, 选取一个进行改写就可以了, 不需要扩展.
    assert(ir.assigned() && ir.get());
    newList->push_back(ir.get());
    const bool use_salt = needsSalt(olk.get());

    // This line implicity handles field aliasing for at least some cases.
    // As i->name can/will be the alias.
    my_addToReturn(&a.rmeta, a.pos++, olk.get(), use_salt, i.name);

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
        my_addSaltToReturn(&a.rmeta, a.pos++);
    }
}



static st_select_lex *
my_rewrite_select_lex(const st_select_lex &select_lex, Analysis &a){
    //do not support filter
    st_select_lex *const new_select_lex = copyWithTHD(&select_lex);
    auto item_it =
        RiboldMYSQL::constList_iterator<Item>(select_lex.item_list);
    List<Item> newList;
    //item的改写, 是写到newlist里面, 所以item本身不会有变化.
    for (;;) {
        const Item *const item = item_it++;
        if (!item)
            break;
        my_rewrite_proj(*item,
                     *constGetAssert(a.rewritePlans, item).get(),
                     a, &newList);
    }
    new_select_lex->item_list = newList;
    return new_select_lex;    
}


static
AbstractQueryExecutor * my_rewrite_select(Analysis &a, LEX *lex){
    LEX *const new_lex = copyWithTHD(lex);
    //this is actually table list instead of join list.
    new_lex->select_lex.top_join_list =
            rewrite_table_list(lex->select_lex.top_join_list, a);
    SELECT_LEX *const select_lex_res = my_rewrite_select_lex(new_lex->select_lex, a);
    set_select_lex(new_lex,select_lex_res);
    return new DMLQueryExecutor(*new_lex, a.rmeta);
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

    auto executor = my_rewrite_select(analysis,lex);
    QueryRewrite *qr = new QueryRewrite(QueryRewrite(true, analysis.rmeta, analysis.kill_zone, executor));

    sp_next(db,query,qr,MygetResTypeFromLuaTable(true));
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
    std::string query1 = "select * from child;"; 
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testCreateTableHandler(item);
        std::cout<<std::endl;
    }

    return 0;
}
