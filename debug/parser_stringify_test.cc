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
#include <main/rewrite_main.hh>
#include "wrapper/reuse.hh"

static std::string embeddedDir="/t/cryt/shadow";

/*convert lex of insert into string*/
static
std::ostream&
simple_insert(std::ostream &out, LEX &lex){
        String s;
        THD *t = current_thd;
        const char* cmd = "INSERT";
        out<<cmd<<" ";
        lex.select_lex.table_list.first->print(t, &s, QT_ORDINARY);
        out << "INTO " << s;
        out << " values " << noparen(lex.many_values);
        return out;
}

static
std::string
convert_insert(const LEX &lex)
{
    std::ostringstream o;
    simple_insert(o,const_cast<LEX &>(lex));
    return o.str();
}

static void testInsertHandler(std::string query,std::string db){
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
    Analysis analysis(std::string(db),*schema,TK,
                        SECURITY_RATING::SENSITIVE);
    std::unique_ptr<query_parse> p;
    p = std::unique_ptr<query_parse>(
                new query_parse(db, query));
    LEX *const lex = p->lex();

    std::cout<<convert_insert(*lex)<<std::endl;
}

int
main(int argc,char**argv) {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";

    if(argc!=2){
        assert(0);        
    }
    std::string db(argv[1]);

//    initFileOnions("conf/already.cnf");

    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    SharedProxyState *shared_ps = 
                     new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::string query1("insert into student values(1,'a\nb\nd\0\0');   ",50);
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testInsertHandler(item,db);
        std::cout<<std::endl;
    }
    return 0;
}
