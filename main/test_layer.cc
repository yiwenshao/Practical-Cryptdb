#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <main/Connect.hh>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>
#include <main/CryptoHandlers.hh>

#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
#include <parser/lex_util.hh>

#include <readline/readline.h>
#include <readline/history.h>

#include <crypto/ecjoin.hh>
#include <util/errstream.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <util/yield.hpp>

#include <sstream>
#include <unistd.h>
#include <map>

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;

std::map<SECLEVEL,std::string> gmp;
std::map<onion,std::string> gmp2;

static const int numOfPipe = 1;

static std::string embeddedDir="/t/cryt/shadow";

//My WrapperState.
class WrapperState {
    WrapperState(const WrapperState &other);
    WrapperState &operator=(const WrapperState &rhs);
    KillZone kill_zone;
public:
    std::string last_query;
    std::string default_db;

    WrapperState() {}
    ~WrapperState() {}
    const std::unique_ptr<QueryRewrite> &getQueryRewrite() const {
        assert(this->qr);
        return this->qr;
    }
    void setQueryRewrite(std::unique_ptr<QueryRewrite> &&in_qr) {
        this->qr = std::move(in_qr);
    }
    void selfKill(KillZone::Where where) {
        kill_zone.die(where);
    }
    void setKillZone(const KillZone &kz) {
        kill_zone = kz;
    }
    
    std::unique_ptr<ProxyState> ps;
    std::vector<SchemaInfoRef> schema_info_refs;

private:
    std::unique_ptr<QueryRewrite> qr;
};

//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;

//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;


static
Item *
my_encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,
                   uint64_t IV) {
    assert(!RiboldMYSQL::is_null(i));
    //这里是onionMeta中的vector, enclayers.也就是洋葱不同层次的加解密通过Onionmeta以及
    //encLary中的加解密算法来完成.
    const auto &enc_layers = om.getLayers();
    assert_s(enc_layers.size() > 0, "onion must have at least one layer");
    const Item *enc = &i;
    Item *new_enc = NULL;
    //这段代码体现了层次加密,也就是说, 通过IV,每个洋葱的层次通过enclayer来表示
    //直接调用其加密和解密函数, 就可以完成加密工作. 加密以后获得的是Item,最后返回加密以后的结果
    for (const auto &it : enc_layers) {
        new_enc = it->encrypt(*enc, IV);
        assert(new_enc);
        enc = new_enc;
    }
    // @i is const, do we don't want the caller to modify it accidentally.
    assert(new_enc && new_enc != &i);
    return new_enc;
}



static
void
my_encrypt_item_all_onions(const Item &i, const FieldMeta &fm,
                        uint64_t IV,std::vector<Item*> *l)
{
    for (auto it : fm.orderedOnionMetas()) {
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;
        //一个fieldmeta表示一个field, 内部的不同洋葱表现在onionMeta,每个onionMeta的不同层次表现
        //在enclyer. 而保持的时候, 是onometekey,onoinmeta这种pair来让我们知道这个onionMeta是哪种
        //枚举的洋葱类型.
        l->push_back(my_encrypt_item_layers(i, o, *om,IV));
    }
}


static
void
my_typical_rewrite_insert_type(const Item &i, const FieldMeta &fm,
                            std::vector<Item *> *l){
    const uint64_t salt = fm.getHasSalt() ? randomValue() : 0;
    my_encrypt_item_all_onions(i, fm, salt,l);
    //对于每种类型, 除了保存加密的洋葱, 还把fm中的salt也变成Int类型保存起来了, 所以会出现奇怪的多了一组数据的情况, 就看
    //这个东西是什么时候应用.
    if (fm.getHasSalt()) {
        l->push_back(new Item_int(static_cast<ulonglong>(salt)));
    }
}



/*
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

*/


static std::unique_ptr<SchemaInfo> myLoadSchemaInfo() {
    //std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::string client="192.168.1.1:1234";
    std::unique_ptr<Connect> e_conn(clients[client]->ps->getEConn().get());

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
    return schema;
}




static Item *
decrypt_item_layers(const Item &i, const FieldMeta *const fm, onion o,
                    uint64_t IV) {
    assert(!RiboldMYSQL::is_null(i));
    const Item *dec = &i;
    Item *out_i = NULL;
    //we have fieldMeta, but only use part of it. we select the onion via the o in olk we constructed.
    const OnionMeta *const om = fm->getOnionMeta(o);
    assert(om);
    //its easy to use onionmeta, just get layers, and use dectypt() to decrypt the results.
    const auto &enc_layers = om->getLayers();
    for (auto it = enc_layers.rbegin(); it != enc_layers.rend(); ++it) {
        out_i = (*it)->decrypt(*dec, IV);
        assert(out_i);
        dec = out_i;
    }
    assert(out_i && out_i != &i);
    return out_i;
}

/*
//structure of return field. 
//map<int,returnField>, int is the index of names
//returnField, represent a field, if the field is not salt, then fieldCalled is the plaintex name

static
ResType decryptResults(const ResType &dbres, const ReturnMeta &rmeta) {
    //num of rows
    const unsigned int rows = dbres.rows.size();
    //num of names, to be decrypted
    const unsigned int cols = dbres.names.size();
    std::vector<std::string> dec_names;

    for (auto it = dbres.names.begin();it != dbres.names.end(); it++){
        const unsigned int index = it - dbres.names.begin();
        //fetch rfmeta based on index
        const ReturnField &rf = rmeta.rfmeta.at(index);
        if (!rf.getIsSalt()) {
            //need to return this field
            //filed name here is plaintext
            dec_names.push_back(rf.fieldCalled());
        }
    }


    const unsigned int real_cols = dec_names.size();

    std::vector<std::vector<Item *> > dec_rows(rows);

    //real cols depends on plain text names.
    for (unsigned int i = 0; i < rows; i++) {
        dec_rows[i] = std::vector<Item *>(real_cols);
    }

    //
    unsigned int col_index = 0;
    for (unsigned int c = 0; c < cols; c++) {
        const ReturnField &rf = rmeta.rfmeta.at(c);
        if (rf.getIsSalt()) {
            continue;
        }

        //the key is in fieldMeta
        FieldMeta *const fm = rf.getOLK().key;

        for (unsigned int r = 0; r < rows; r++) {
	    //
            if (!fm || dbres.rows[r][c]->is_null()) {
                dec_rows[r][col_index] = dbres.rows[r][c];
            } else {

                uint64_t salt = 0;
                const int salt_pos = rf.getSaltPosition();
                //read salt from remote datab for descrypting.
                if (salt_pos >= 0) {
                    Item_int *const salt_item =
                        static_cast<Item_int *>(dbres.rows[r][salt_pos]);
                    assert_s(!salt_item->null_value, "salt item is null");
                    salt = salt_item->value;
                }

                 //specify fieldMeta, onion, and salt should be able to decrpyt
                //peel onion
                dec_rows[r][col_index] =
                    decrypt_item_layers(*dbres.rows[r][c],fm,rf.getOLK().o,salt);
            }
        }
        col_index++;
    }
    //resType is used befor and after descrypting.
    return ResType(dbres.ok, dbres.affected_rows, dbres.insert_id,
                   std::move(dec_names),
                   std::vector<enum_field_types>(dbres.types),
                   std::move(dec_rows));
}

*/

//first step of back
static std::vector<FieldMeta *> getFieldMeta(SchemaInfo &schema,std::string db = "tdb",
                                                               std::string table="student1"){
     const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
     Analysis analysis(db,schema,TK,
                        SECURITY_RATING::SENSITIVE);
     if(analysis.databaseMetaExists(db)){
        const DatabaseMeta & dbm = analysis.getDatabaseMeta(db);
        TableMeta & tbm = *dbm.getChild(IdentityMetaKey(table));
    	return tbm.orderedFieldMetas();
     }else{
         std::cout<<"data base not exists"<<std::endl;
	 return std::vector<FieldMeta *>();
     }
}

static
Item* getIntItem(int i){
    return new Item_int(100);
}

static
Item* getStringItem(string s){
    return new Item_string(make_thd_string(s),s.length(),&my_charset_bin);
}


static
void testEncrypt(SchemaInfo &schema){
    return;
    string db="tdb",table="student";

    //get all the fields in the tables.
    std::vector<FieldMeta*> fms = getFieldMeta(schema,db,table);
    //try item_int here
    Item * iint = new Item_int(100);
    string s = "hehe";
    THD *thd = current_thd;
    assert(thd);
    //Item *is = new Item_string(make_thd_string(s),s.length(),&my_charset_bin);
    std::vector<Item *> l;
    my_typical_rewrite_insert_type(*iint,*fms[0],&l);

    Item * is = getStringItem("zhao");
    if(is==NULL){}
    std::vector<Item *> l2;
    my_encrypt_item_all_onions(*getIntItem(100),*fms[0],100,&l2);
}

//oDET,oOPE,oAGG,

static 
void testEncrypt2(SchemaInfo &schema){
    string db="tdb",table="student";
    //get all the fields in the tables.
    std::vector<FieldMeta*> fms = getFieldMeta(schema,db,table);
    Item * iint = new Item_int(100);
    OnionMeta *om = fms[0]->getOnionMeta(oDET);
    Item* enc = my_encrypt_item_layers(*iint,oDET,*om,110);
    Item* dec = decrypt_item_layers(*enc,fms[0],oDET,110);
    String s;
    //for string, the result will automatically be escaped.
    dec->print(&s, QT_ORDINARY);
    cout<<string(s.ptr(), s.length())<<endl;
}

static 
void myAdjustOnion(){



}


int
main(int argc, char* argv[]) {
     if(argc!=3){
         for(int i=0;i<argc;i++){
             printf("%s\n",argv[i]);
         }
         return 0;
     }
     string db(argv[1]),table(argv[2]);
     cout<<db<<":"<<table<<endl;


     gmp[SECLEVEL::INVALID]="INVALID";
     gmp[SECLEVEL::PLAINVAL]="PLAINVAL";
     gmp[SECLEVEL::OPE]="OPE";
     gmp[SECLEVEL::DETJOIN]="DETJOIN";
     gmp[SECLEVEL::OPEFOREIGN]="OPEFOREIGN";
     gmp[SECLEVEL::DET]="DET";
     gmp[SECLEVEL::SEARCH]="SEARCH";
     gmp[SECLEVEL::HOM]="HOM";
     gmp[SECLEVEL::RND]="RND";
     gmp2[oDET]="oDET";
     gmp2[oOPE]="oOPE";
     gmp2[oAGG]="oAGG";
     gmp2[oSWP]="oSWP";
     gmp2[oPLAIN]="oPLAIN";
     gmp2[oBESTEFFORT]="oBESTEFFORT";
     gmp2[oINVALID]="oINVALID";

    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();    
    //Connect phase
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    const std::string master_key = "113341234";
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){  
        perror("getcwd error");  
    }
    embeddedDir = std::string(buffer)+"/shadow";

    SharedProxyState *shared_ps = 
			new SharedProxyState(ci, embeddedDir , master_key, 
                                            determineSecurityRating());
    assert(0 == mysql_thread_init());

    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    clients[client]->ps->safeCreateEmbeddedTHD();

    //Connect end!!
    globalConn = new Connect(ci.server, ci.user, ci.passwd, ci.port);

    THD *thd = current_thd;

    assert(thd);
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();

    assert(0 == mysql_thread_init());

    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    clients[client]->ps->safeCreateEmbeddedTHD();

    thd = current_thd;
    if(schema.get()==NULL){}
    assert(thd);
    testEncrypt(*schema);
    testEncrypt2(*schema);
    myAdjustOnion();
    return 0;
}
