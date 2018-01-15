#pragma once

/*1. store data as column files, and restore data as plaintext insert query
* 2. plaintext insert query should be able to recover directly
* 3. should be able to used exsisting data to reduce the computation overhead(to be implemented)
*/
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;

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

static std::string embeddedDir="/t/cryt/shadow";
//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;
//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;

//Return values got by using directly the MySQL c Client
struct rawMySQLReturnValue {
    std::vector<std::vector<std::string>> rowValues;/*data tuples*/
    std::vector<std::string> fieldNames;
    std::vector<enum_field_types> fieldTypes;
    std::vector<int> lengths;
    std::vector<int> maxlengths;/* what's the difference 
                                   between length and maxlength?*/
    std::vector<int> choosen_onions;
 
    void show();
};


void rawMySQLReturnValue::show(){
    cout<<"rowvalues:"<<endl;
    for(auto item_vec:rowValues){
        for(auto item:item_vec){
            cout<<item.size()<<"\t";
        }
        cout<<endl;
    }
    cout<<"types:"<<endl;
    for(auto item:fieldTypes){
        cout<<IS_NUM(item)<<"\t";
    }
    cout<<endl;
    cout<<"fieldNames:"<<endl;
    for(auto item:fieldNames){
        cout<<item<<"\t";
    }
    cout<<endl;
    cout<<"lengths:"<<endl;
    for(auto item:lengths){
        cout<<item<<"\t";
    }
    cout<<endl;
    cout<<"maxlengths:"<<endl;
    for(auto item:maxlengths){
       cout<<item<<"\t";
    }
    cout<<endl;
}

//representation of one field.
struct transField{
    bool hasSalt;
    FieldMeta *originalFm;
    vector<int> choosenOnions;
    //used to construct return meta
    int onionIndex = 0;
    int numOfOnions=0;
    //onions
    std::vector<std::string> fields;
    std::vector<onion> onions;
//    std::vector<OnionMeta*>originalOm;
};


static void init(){
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
}




//must be static, or we get "no previous declaration"
//execute the query and get the rawReturnVale, this struct can be copied.
static 
rawMySQLReturnValue executeAndGetResultRemote(Connect * curConn,std::string query){
    std::unique_ptr<DBResult> dbres;
    curConn->execute(query, &dbres);
    rawMySQLReturnValue myRaw;
    
    if(dbres==nullptr||dbres->n==NULL){
        std::cout<<"no results"<<std::endl;
        return myRaw;
    }

    int num = mysql_num_rows(dbres->n);
    
    int numOfFields = mysql_num_fields(dbres->n);

    MYSQL_FIELD *field;
    MYSQL_ROW row;

    if(num!=0){
        while( (row = mysql_fetch_row(dbres->n)) ){
            //what's the difference between fieldlen
	    unsigned long * fieldLen = mysql_fetch_lengths(dbres->n);
            std::vector<std::string> curRow;
            for(int i=0;i<numOfFields;i++){
                if (i == 0) {
                    while( (field = mysql_fetch_field(dbres->n)) ) {
                        myRaw.fieldNames.push_back(std::string(field->name));
                        myRaw.fieldTypes.push_back(field->type);
                        //myRaw.lengths.push_back(field->length);
                        //myRaw.lengths.push_back(fieldLen[i]);
                        myRaw.lengths.push_back(field->max_length);
                        myRaw.maxlengths.push_back(field->max_length);
                        //cout<<field->length<<"::"<<field->max_length<<endl;
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






//first step of back
static 
std::vector<FieldMeta *> 
getFieldMeta(SchemaInfo &schema,
             std::string db = "tdb",
             std::string table="student") {
     const std::unique_ptr<AES_KEY> &TK = 
                 std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
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

    Analysis analysis(std::string("student"),*schema,
                      std::unique_ptr<AES_KEY>(getKey(std::string("113341234"))),
                        SECURITY_RATING::SENSITIVE);
    return schema;
}

/*for each field, convert the format to transField*/
static std::vector<transField> getTransField(std::vector<FieldMeta *> pfms){
    std::vector<transField> res;
    //for every field
    for(auto pfm:pfms){
        transField tf;
	tf.originalFm = pfm;
        for(std::pair<const OnionMetaKey *, OnionMeta *> &ompair:pfm->orderedOnionMetas()){
            tf.fields.push_back((ompair.second)->getAnonOnionName());
            tf.onions.push_back(ompair.first->getValue());
        }
        if(pfm->getHasSalt()){
            tf.hasSalt=true;
	    tf.fields.push_back(pfm->getSaltName());
        }
        res.push_back(tf);
    }
    return res;
}


//query for testing purposes
static
std::string getTestQuery(SchemaInfo &schema, std::vector<transField> &tfds,
                                     std::string db="tdb",std::string table="student1"){
    std::string res = "SELECT ";
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(db));
    //get databaseMeta, search in the map
    DatabaseMeta * dbm = schema.getChild(*dbmeta_key);
    const TableMeta & tbm = *((*dbm).getChild(IdentityMetaKey(table)));
    std::string annotablename = tbm.getAnonTableName();

    //then a list of onion names
    for(auto item:tfds){
        for(auto index:item.choosenOnions){
            res += item.fields[index];
            res += " , ";
        }
    	if(item.hasSalt){
            res += item.originalFm->getSaltName()+" , ";
        }
    }

    res = res.substr(0,res.size()-2);
    res = res + "FROM `"+db+std::string("`.`")+annotablename+"`";
    return res;
}


