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
        return ResType(true, 0 ,
                               in_last_insert_id, std::move(names),
                                   std::move(types), std::move(rows));
    }
}





static 
AbstractQueryExecutor *
myRewriteAndUpdate(Analysis &a, LEX *lex, std::string db,std::string table){
    assert(a.deltas.size() == 0);
    TEST_DatabaseDiscrepancy(db, a.getDatabaseName());
    LEX *const new_lex = copyWithTHD(lex);
    if (false == a.tableMetaExists(db, table)){
        std::unique_ptr<TableMeta> tm(new TableMeta(true, true));
        TEST_Text(1 == new_lex->select_lex.table_list.elements,
                  "we do not support multiple tables in a CREATE"
                  " TABLE queries");
        // Take the table name straight from 'tm' as
        // Analysis::getAnonTableName relies on SchemaInfo.
        TABLE_LIST *const tbl =
            rewrite_table_list(new_lex->select_lex.table_list.first,
                               tm->getAnonTableName());
        //new table_list only contain one element
        new_lex->select_lex.table_list =
            *oneElemListWithTHD<TABLE_LIST>(tbl);

        // collect the keys (and their types) as they may affect the onion
        // layout we use
        const auto &key_data = collectKeyData(*lex);
        auto it =
            List_iterator<Create_field>(lex->alter_info.create_list);
        new_lex->alter_info.create_list =
            accumList<Create_field>(it,
                [&a, &tm, &key_data] (List<Create_field> out_list,
                                      Create_field *const cf) {
                    return createAndRewriteField(a, cf, tm.get(),
                                                 true, key_data, out_list);
            });
        // -----------------------------
        //         Rewrite INDEX
        // -----------------------------
        highLevelRewriteKey(*tm.get(), *lex, new_lex, a);        
        a.deltas.push_back(std::unique_ptr<Delta>(
                            new CreateDelta(std::move(tm),
                                            a.getDatabaseMeta(db),
                                            IdentityMetaKey(table))));
    }else{
        std::cout<<"error"<<std::endl;
        return NULL;
    }
    return new DDLQueryExecutor(*new_lex, std::move(a.deltas));
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
//    std::cout<<RED_BEGIN<<"rd.affected_rows: "<<rd.affected_rows<<COLOR_END<<std::endl;
//    std::cout<<RED_BEGIN<<"rd.insert_id: "<<rd.insert_id<<COLOR_END<<std::endl;

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
    //test_Analysis(analysis);
//    DDLHandler *h = new CreateTableHandler();
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse(db, query));
    LEX *const lex = p->lex();

    std::string table(lex->select_lex.table_list.first->table_name);
    auto executor = myRewriteAndUpdate(analysis,lex,db,table);
    //unique ptr is ok, while shared_ptr will cause internal compiler bug
    QueryRewrite *qr = new QueryRewrite(QueryRewrite(true, analysis.rmeta, analysis.kill_zone, executor));
                                       ;
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
    std::string query1 = "CREATE TABLE child (id integer,name varchar(20))"; 
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testCreateTableHandler(item);
        std::cout<<std::endl;
    }

    return 0;
}
