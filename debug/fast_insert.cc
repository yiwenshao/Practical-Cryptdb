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

//std::map<onion,std::vector<std::string>> fileOnions;

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


/************************************************************************************************/

/*encrypt one onion to get item. if the onion exists, then return directly*/
static Item *
my_encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                    const Analysis &a, uint64_t IV) {
    assert(!RiboldMYSQL::is_null(i));
    const auto &enc_layers = a.getEncLayers(om);
    assert_s(enc_layers.size() > 0, "onion must have at least one layer");
    const Item *enc = &i;
    Item *new_enc = NULL;
    for (const auto &it : enc_layers) {
        new_enc = it->encrypt(*enc, IV);
        assert(new_enc);
        enc = new_enc;
    }
    assert(new_enc && new_enc != &i);
    return new_enc;
}


/*encrypt the item to get a set of onions, in the parater l,salt is also added*/
static
void
my_typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                            Analysis &a, std::vector<Item *> *l) {
    const uint64_t salt = fm.getHasSalt() ? randomValue() : 0;
    uint64_t IV = salt;
    for (auto it : fm.orderedOnionMetas()) {
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;      
//        if(o==oAGG){
//            std::vector<std::string> &tempFileVector = fileOnions[o];
//            std::string in(tempFileVector.back());
//            tempFileVector.pop_back();
//            l->push_back(MySQLFieldTypeToItem(static_cast<enum_field_types>(253),in));
//        }else{
            l->push_back(my_encrypt_item_layers(i, o, *om, a, IV));
//        }
    }
    if (fm.getHasSalt()) {
        l->push_back(new Item_int(static_cast<ulonglong>(salt)));
    }
}

static
void myRewriteInsertHelper(const Item &i, const FieldMeta &fm, Analysis &a,
                           List<Item> *const append_list){
    std::vector<Item *> l;
    my_typical_rewrite_insert_type(i,fm,a,&l);
    for (auto it : l) {
        append_list->push_back(it);
    }
}

static std::string getInsertResults(Analysis a,LEX* lex){
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
        std::vector<Item *> implicit_defaults;
        
        // No field list, use the table order.
        assert(fmVec.empty());
        std::vector<FieldMeta *> fmetas = tm.orderedFieldMetas();
        fmVec.assign(fmetas.begin(), fmetas.end());

        if (lex->many_values.head()) {
            //start processing many values
            auto it = List_iterator<List_item>(lex->many_values);
            List<List_item> newList;
            for (;;) {
                List_item *const li = it++;
                if (!li) {
                    break;
                }
                List<Item> *const newList0 = new List<Item>();
                if (li->elements != fmVec.size()) {
                    TEST_TextMessageError(0 == li->elements
                                         && NULL == lex->field_list.head(),
                                          "size mismatch between fields"
                                          " and values!");
                } else {
                    auto it0 = List_iterator<Item>(*li);
                    auto fmVecIt = fmVec.begin();

                    for (;;) {
                        const Item *const i = it0++;
                        assert(!!i == (fmVec.end() != fmVecIt));
                        if (!i) {
                            break;
                        }
                        //fetch values, and use fieldMeta to facilitate rewrite
                        //every filed should be encrypted with onions of encryption
                        myRewriteInsertHelper(*i, **fmVecIt, a, newList0);
                        ++fmVecIt;
                    }
                    for (auto def_it : implicit_defaults) {
                        newList0->push_back(def_it);
                    }
                }
                newList.push_back(newList0);
            }
            new_lex->many_values = newList;
        }
        return convert_insert(*new_lex);
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

    std::cout<<getInsertResults(analysis,lex)<<std::endl;
}

//fileOnions
/*
static 
void initFileOnions(std::string filename,std::string dataPrefix="allTables/table_MGVGBABDMU/"){
    std::ifstream infile(filename);
    std::string line;
    
    std::string onionAnnoName;
    while(getline(infile,line)){
        int index = line.find(":");
        std::string head = line.substr(0,index);
        std::string next = line.substr(index+1);
        onionAnnoName = next;
    }

    std::vector<std::string> res;
    load_string_file(dataPrefix+onionAnnoName,res,256);
    std::reverse(res.begin(),res.end());
    fileOnions[oAGG] = std::move(res);
}*/

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
    //std::string query1 = "insert into student values(1,\"ZHAOYUN\"),(2,'XC'),(3,'KK')";
    std::string query1 = "insert into student values(NULL)";
    std::vector<std::string> querys{query1};
    for(auto item:querys){
        std::cout<<item<<std::endl;
        testInsertHandler(item,db);
        std::cout<<std::endl;
    }
    return 0;
}
