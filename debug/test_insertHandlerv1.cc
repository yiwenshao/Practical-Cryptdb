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

static void testInsertHandler(std::string query){
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
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    //just like what we do in Rewrite::rewrite,dispatchOnLex
    Analysis analysis(std::string("tdb"),*schema,TK,
                        SECURITY_RATING::SENSITIVE);
    DMLHandler *h = new InsertHandler();
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse("tdb", query));
    LEX *const lex = p->lex();

    auto executor = h->transformLex(analysis,lex);
    std::cout<<((DMLQueryExecutor*)executor)->getQuery()<<std::endl;
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
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::string query1 = "insert into student values(1,\"zhangfei\")";
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testInsertHandler(item);
        std::cout<<std::endl;
    }
    return 0;
}
