#include <string.h>
#include <stdio.h>
#include <unistd.h>
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
#include "util/util.hh"
#include "redisbio/zmalloc.h"
#include "redisbio/adlist.h"
#include "redisbio/bio.h"

extern CItemTypesDir itemTypes;
static std::string embeddedDir="/t/cryt/shadow";
SchemaInfo *gschema;
AES_KEY *gkey;
std::string gdb="tdb";
Analysis *ganalysis = NULL;

//expand the item
template <typename ContainerType>
void myRewriteInsertHelper(const Item &i, const FieldMeta &fm, Analysis &a,
                         ContainerType *const append_list){
    std::vector<Item *> l;
    itemTypes.do_rewrite_insert(i, fm, a, &l);
    for (auto it : l) {
        append_list->push_back(it);
    }
}

static std::string 
getInsertResults(Analysis a,LEX* lex){
        std::cout<<"###################"<<std::endl;
        if(lex==NULL) return "aa";
        LEX *const new_lex = copyWithTHD(lex);
        const std::string &table =
            lex->select_lex.table_list.first->table_name;
        const std::string &db_name =
            lex->select_lex.table_list.first->db;
        //from databasemeta to tablemeta.
        const TableMeta &tm = a.getTableMeta(db_name, table);
        //rewrite table name
        new_lex->select_lex.table_list.first =
            rewrite_table_list(lex->select_lex.table_list.first, a);
        std::vector<FieldMeta *> fmVec;
        std::vector<FieldMeta *> fmetas = tm.orderedFieldMetas();
        fmVec.assign(fmetas.begin(), fmetas.end());
        if (lex->many_values.head()) {
            auto it = List_iterator<List_item>(lex->many_values);
            List<List_item> newList;
            for (;;) {
                List_item *const li = it++;
                if (!li) {
                    break;
                }
                List<Item> *const newList0 = new List<Item>();
                if (li->elements != fmVec.size()) {
                    exit(0);
                } else {
                    auto it0 = List_iterator<Item>(*li);
                    auto fmVecIt = fmVec.begin();
                    for (;;) {
                        const Item *const i = it0++;
                        assert(!!i == (fmVec.end() != fmVecIt));
                        if (!i) {
                            break;
                        }
                        myRewriteInsertHelper(*i, **fmVecIt, a, newList0);
                        ++fmVecIt;
                    }
                }
                newList.push_back(newList0);
            }
            new_lex->many_values = newList;
        }
        return lexToQuery(*new_lex);
        return "aa";
}

static
int
ginsertFunction(unsigned long id,void *input){
    struct bio_job *task = (struct bio_job *)input;
    if(task->stop == 1) return 1;
    LEX * lex = (LEX*)(task->arg1);
    std::cout<<getInsertResults(*ganalysis,lex)<<std::endl;
    (void)lex;
    return 0;
}

static
void
testInsertHandler(){
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    gschema = new SchemaInfo();
    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }
            return parent;
        };
    loadChildren(gschema); 
    gkey = getKey(std::string("113341234"));
}

int
main(){
    userFunction = ginsertFunction;
    bioInit();
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    //Free to remove memory leak        
    embeddedDir = std::string(buffer)+"/shadow";
    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    UNUSED(ci);
    //Clean!!
    free(buffer);
    //this function alone has memory leak
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    UNUSED(testInsertHandler);
    UNUSED(getInsertResults);
    std::string query1 = "insert into student values(1,\"zhangfei\")";
    std::vector<std::string> queries{query1};
    for(unsigned int i=0u;i<100u;i++){
        queries.push_back(query1);
    }   
    testInsertHandler();
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(gkey);
    Analysis analysis(gdb,*gschema,TK,
                        SECURITY_RATING::SENSITIVE);
    ganalysis = &analysis;

    std::vector<LEX*> qa(100,NULL);

    //send work
    int type = 0;    
    for(int i=0;i<10;i++) {
         query_parse * p = new query_parse(gdb, queries[i]);
         LEX *const lex = p->lex();
         qa[i]=lex;
    }

    for(int i=0;i<10;i++) {
         bioCreateBackgroundJob(type,qa[i],NULL,0);
         type+=1;
         type%=10;
    }



    //Only ten threads to terminate
    for(int i=0;i<10;i++) {
        bioCreateBackgroundJob(type,NULL,NULL,1);
        type+=1;
        type%=10;
    }
    printf("tobe joined\n");
    bioKillThreads();
    return 0;
}
