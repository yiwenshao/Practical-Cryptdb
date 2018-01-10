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



static void testCreateTableHandler(std::string query){
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
                new query_parse("tdb", query));
    LEX *const lex = p->lex();

    auto executor = myRewriteAndUpdate(analysis,lex,"tdb","child");
//    auto executor = h->transformLex(analysis,lex);
    std::cout<<  ((DDLQueryExecutor*)executor)->new_query<<std::endl;
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
    SharedProxyState *shared_ps = 
        new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);

    std::string query1 = "CREATE TABLE child (id integer,name varchar(20))"; 
    //std::string query1 = "CREATE TABLE child (id decimal)";
    std::string query2 = "CREATE TABLE child (id tinyint)";
    std::string query3 = "CREATE TABLE child (id mediumint)";
    std::string query4 = "CREATE TABLE child (id smallint)";
    std::string query5 = "CREATE TABLE child (id int)";
    std::string query6 = "CREATE TABLE child (id bigint)";
    std::string query7 = "CREATE TABLE child (name varchar(100))";
    std::string query8 = "CREATE TABLE child (name varchar(1000))";
    std::string query9 = "CREATE TABLE child (name varchar(10000))";

    std::vector<std::string> querys{query1,query2,query3,query4,query5,query6,query7,query8,query9};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testCreateTableHandler(item);
        std::cout<<std::endl;
    }

    return 0;
}
