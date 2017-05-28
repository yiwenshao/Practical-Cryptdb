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
using std::string;
std::map<SECLEVEL,std::string> gmp;
std::map<onion,std::string> gmp2;


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

//Return values got by using directly the MySQL c Client
struct rawReturnValue{
    std::vector<std::vector<std::string> > rowValues;
    std::vector<std::string> fieldNames;
    std::vector<int> fieldTypes;
};

//must be static, or we get "no previous declaration"
//execute the query and getthe rawReturnVale, this struct can be copied.
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

//print RawReturnValue for testing purposes.
static
void printrawReturnValue(rawReturnValue & cur) {
    int len = cur.fieldTypes.size();
    if(len==0){
        //std::cout<<"zero output"<<std::endl;
        return ;
    }

    if(static_cast<int>(cur.fieldNames.size())!=len||static_cast<int>(cur.rowValues[0].size())!=len){
        std::cout<<RED_BEGIN<<"size mismatch in printrawReturnValue"<<COLOR_END<<std::endl;
        return ;
    }

    for(int i=0;i<len;i++){
        std::cout<<cur.fieldNames[i]<<":"<<cur.fieldTypes[i]<<"\t";
    }

    std::cout<<std::endl;
    for(auto row:cur.rowValues){
        for(auto rowItem:row){
            std::cout<<rowItem<<"\t";
        }
        std::cout<<std::endl;
    }
}

//The rewrite phase of cryptdb.

/*
1. getSchemaInfo
2. rewrite => gather/write
3. fetch the executor and put it in wrapperState(std::unique_ptr<QueryRewrite> qr) 
*/

static
bool myRewrite(std::string curQuery,unsigned long long _thread_id,std::string client) {
    assert(0 == mysql_thread_init());
    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = c_wrapper->ps.get();
    assert(ps);
    c_wrapper->last_query = curQuery;
    //std::cout<<RED_BEGIN<<"start my rewrite"<<COLOR_END<<std::endl;
    try{
        TEST_Text(retrieveDefaultDatabase(_thread_id, ps->getConn(),
                                              &c_wrapper->default_db),
                                  "proxy failed to retrieve default database!");
        const std::shared_ptr<const SchemaInfo> &schema =  ps->getSchemaInfo();
        c_wrapper->schema_info_refs.push_back(schema);
        std::unique_ptr<QueryRewrite> qr =
            std::unique_ptr<QueryRewrite>(new QueryRewrite(
                    Rewriter::rewrite(curQuery, *schema.get(),
                                      c_wrapper->default_db, *ps)));
        assert(qr);
        c_wrapper->setQueryRewrite(std::move(qr));
        }catch(...){
            std::cout<<"rewrite exception!!!"<<std::endl;
            return false;
        }
        return true;
}

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

//transform rawReturnValue to ResType
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
        //uint64_t afrow = globalConn->get_affected_rows();
	//std::cout<<GREEN_BEGIN<<"Affected rows: "<<afrow<<COLOR_END<<std::endl;
        return ResType(true, 0 ,
                               in_last_insert_id, std::move(names),
                                   std::move(types), std::move(rows));
    }
}

//printResType for testing purposes
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


//the "next" phase of cryptdb
/*
1. call function "next" in the executor
2. process three different return types.
*/

static
void myNext(std::string client,bool isFirst,ResType inRes) {
    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = c_wrapper->ps.get();
    assert(ps);
    ps->safeCreateEmbeddedTHD();
   
    const ResType &res = inRes;
    const std::unique_ptr<QueryRewrite> &qr = c_wrapper->getQueryRewrite();

    try{
        NextParams nparams(*ps, c_wrapper->default_db, c_wrapper->last_query);
        const auto &new_results = qr->executor->next(res, nparams);
        const auto &result_type = new_results.first;
        switch (result_type){
            //execute the query, fetch the results, and call next again
        case AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN: {
            //std::cout<<RED_BEGIN<<"case one"<<COLOR_END<<std::endl;
            const auto &output =
                std::get<1>(new_results)->extract<std::pair<bool, std::string> >();
            const auto &next_query = output.second;
            //here we execute the query against the remote database, and get rawReturnValue
            rawReturnValue resRemote = executeAndGetResultRemote(globalConn,next_query);
            //transform rawReturnValue first
            const auto &againGet = MygetResTypeFromLuaTable(false,&resRemote);            
            myNext(client,false,againGet);
            break;
        }

        //only execute the query, without processing the retults
        case AbstractQueryExecutor::ResultType::QUERY_USE_RESULTS:{
            //std::cout<<RED_BEGIN<<"case two"<<COLOR_END<<std::endl;
            const auto &new_query =
                std::get<1>(new_results)->extract<std::string>();
            auto resRemote = executeAndGetResultRemote(globalConn,new_query);
            printrawReturnValue(resRemote);
            break;
        }

        //return the results to the client directly 
        case AbstractQueryExecutor::ResultType::RESULTS:{
            //std::cout<<RED_BEGIN<<"case three"<<COLOR_END<<std::endl;
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
void batchTogether(std::string client, std::string curQuery,unsigned long long _thread_id) {
    //the first step is to Rewrite, we abort this session if we fail here.
    bool resMyRewrite =  myRewrite(curQuery,_thread_id,client);
    if(!resMyRewrite){
         //std::cout<<"my rewrite error in batch"<<std::endl;
         return ; 
    }
    myNext(client,true, MygetResTypeFromLuaTable(true));
}



static void processLayers(const EncLayer &enc){
    //std::cout<<enc.serialize(enc)<<std::endl;
    std::cout<<enc.name()<<std::endl;
}



static void processOnionMeta(const OnionMeta &onion){
    std::cout<<GREEN_BEGIN<<"PRINT OnionMeta"<<COLOR_END<<std::endl;
    std::cout<<"onionmeta->getAnonOnionName(): "<<onion.getAnonOnionName()<<std::endl;
    auto &layers = onion.getLayers();
    for(auto &slayer:layers){
        processLayers(*(slayer.get()));
    }
}



static void processFieldMeta(const FieldMeta &field){
//Process general info
    if(field.getHasSalt()){
        std::cout<<"this field has salt"<<std::endl;
    }
    std::cout<<"field.getFieldName(): "<<field.getFieldName()<<std::endl;
    std::cout<<"field.getSaltName(): "<<field.getSaltName()<<std::endl;
    std::cout<<"field.serialize(): "<<field.serialize(field)<<std::endl;

    for(std::pair<const OnionMetaKey *, OnionMeta *> &ompair:field.orderedOnionMetas()){
	processOnionMeta(*ompair.second);
    }
//Process Onions
    if(field.hasOnion(oDET)){
        field.getOnionMeta(oDET);
    }
    if(field.hasOnion(oOPE)){
	field.getOnionMeta(oOPE);
    }
    if(field.hasOnion(oAGG)){
	field.getOnionMeta(oAGG);
    }
    return;
//iterate over onions
    for(const std::pair<const OnionMetaKey,std::unique_ptr<OnionMeta> > & onion: field.getChildren()){
        std::cout<<onion.second->getDatabaseID()<<":"<<onion.first.getValue()<<std::endl;
    }
}


static void processTableMeta(const TableMeta &table){
    std::cout<<GREEN_BEGIN<<"PRINT TableMeta"<<COLOR_END<<std::endl;
    for(FieldMeta *cfm:table.orderedFieldMetas()){
	processFieldMeta(*cfm);
    }
}


static void processDatabaseMeta(const DatabaseMeta & dbm,std::string table="student1") {
    TableMeta & tbm = *dbm.getChild(IdentityMetaKey(table));
    processTableMeta(tbm);
    return;

}

static void processSchemaInfo(SchemaInfo &schema,std::string db="tdb"){
     const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
     Analysis analysis(db,schema,TK,
                        SECURITY_RATING::SENSITIVE);
     if(analysis.databaseMetaExists(db)){
         processDatabaseMeta(analysis.getDatabaseMeta(db));
     }else{
	 std::cout<<"data base not exists"<<std::endl;
     }
}

static std::unique_ptr<SchemaInfo> myLoadSchemaInfo() {
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

    Analysis analysis(std::string("student"),*schema,std::unique_ptr<AES_KEY>(getKey(std::string("113341234"))),
                        SECURITY_RATING::SENSITIVE);
    return schema;
}

int
main(int argc,char ** argv) {
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

     string targetDb;
     if(argc==2){
        targetDb = string(argv[1]);
     }

    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();    
    //Connect phase
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    //const std::string master_key = "113341234";
    const std::string master_key = "113341234HEHE";

    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){  
        perror("getcwd error");  
    }
    embeddedDir = std::string(buffer)+"/shadow";


    SharedProxyState *shared_ps = 
			new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(0 == mysql_thread_init());
    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    clients[client]->ps->safeCreateEmbeddedTHD();
    //Connect end!!
    globalConn = new Connect(ci.server, ci.user, ci.passwd, ci.port);
    std::string curQuery = "SHOW DATABASES;";
    std::cout<<"please input a new query:######################################################"<<std::endl;
    if(targetDb.size()==0)
        std::getline(std::cin,curQuery);
    else curQuery = string("use ")+targetDb;
    unsigned long long _thread_id = globalConn->get_thread_id();
    long long countWrapper = 0;
    while(curQuery!="quit"){
        if(curQuery.size()==0){
            std::cout<<std::endl;
            std::getline(std::cin,curQuery);
            std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
            processSchemaInfo(*schema);
            continue;
        }
        countWrapper++;
        batchTogether(client,curQuery,_thread_id);
        std::cout<<GREEN_BEGIN<<"\nplease input a new query:#######"<<COLOR_END<<std::endl;
        std::getline(std::cin,curQuery);
        if(countWrapper==2){
            cout<<"bingo"<<endl;
            countWrapper=0;
        }
    }
    return 0;
}
