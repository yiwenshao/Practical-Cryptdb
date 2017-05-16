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



struct backupOnionSelection {
    int numOfFields;
    std::vector<int> fieldSize;
    std::vector<int> saltIndex;
    std::vector<int> onionIndex;
    backupOnionSelection(int n):numOfFields(n),fieldSize(n,-1),saltIndex(n,-1),onionIndex(n,-1){}
    void print();
};




//must be static, or we get "no previous declaration"
//execute the query and getthe rawReturnVale, this struct can be copied.
static 
rawReturnValue executeAndGetResultRemote(Connect * curConn,std::string query){
    std::unique_ptr<DBResult> dbres;
    curConn->execute(query, &dbres);
    rawReturnValue myRaw;
    
    if(dbres==nullptr||dbres->n==NULL){
        std::cout<<"no results"<<std::endl;
        return myRaw;
    }

    int num = mysql_num_rows(dbres->n);
    std::cout<<"num of rows: "<<num<<std::endl;
    
    int numOfFields = mysql_num_fields(dbres->n);
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
        std::cout<<"zero output"<<std::endl;
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
    std::cout<<RED_BEGIN<<"start my rewrite"<<COLOR_END<<std::endl;      

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
make_null(const std::string &name = "")
{
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
        uint64_t afrow = globalConn->get_affected_rows();
	std::cout<<GREEN_BEGIN<<"Affected rows: "<<afrow<<COLOR_END<<std::endl;
        return ResType(true, 0 ,
                               in_last_insert_id, std::move(names),
                                   std::move(types), std::move(rows));
    }
}

//printResType for testing purposes
static 
void parseResType(const ResType &rd) {
    std::cout<<RED_BEGIN<<"rd.affected_rows: "<<rd.affected_rows<<COLOR_END<<std::endl;
    std::cout<<RED_BEGIN<<"rd.insert_id: "<<rd.insert_id<<COLOR_END<<std::endl;
    
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
            std::cout<<RED_BEGIN<<"case one"<<COLOR_END<<std::endl;
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
            std::cout<<RED_BEGIN<<"case two"<<COLOR_END<<std::endl;
            const auto &new_query =
                std::get<1>(new_results)->extract<std::string>();
            auto resRemote = executeAndGetResultRemote(globalConn,new_query);
            printrawReturnValue(resRemote);
            break;
        }

        //return the results to the client directly 
        case AbstractQueryExecutor::ResultType::RESULTS:{
            std::cout<<RED_BEGIN<<"case three"<<COLOR_END<<std::endl;
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
         std::cout<<"my rewrite error in batch"<<std::endl;
         return ; 
    }
    myNext(client,true, MygetResTypeFromLuaTable(true));
}


static void processFieldMeta(const FieldMeta &field){
    std::cout<<GREEN_BEGIN<<"PRINT FieldMeta"<<COLOR_END<<std::endl;
    for(const std::pair<const OnionMetaKey,std::unique_ptr<OnionMeta> > & onion: field.getChildren()){
        std::cout<<onion.second->getDatabaseID()<<":"<<onion.first.getValue()<<std::endl;
    }
    std::cout<<GREEN_BEGIN<<"end FieldMeta"<<COLOR_END<<std::endl;
}

static void processTableMeta(const TableMeta &table){
    std::cout<<GREEN_BEGIN<<"PRINT TableMeta"<<COLOR_END<<std::endl;
    for(const std::pair<const IdentityMetaKey,std::unique_ptr<FieldMeta> > & field: table.getChildren()){
        std::cout<<field.second->getDatabaseID()<<":"<<field.first.getValue()<<std::endl;
        processFieldMeta(*(field.second));
    }
}


static void processDatabaseMeta(const DatabaseMeta & dbm,std::string table="student1") {
    TableMeta & tbm = *dbm.getChild(IdentityMetaKey(table));
    processTableMeta(tbm);
    return;

    std::cout<<GREEN_BEGIN<<"PRINT DatabaseMeta"<<COLOR_END<<std::endl;
    for(const std::pair<const IdentityMetaKey,std::unique_ptr<TableMeta> > & table: dbm.getChildren()){
        processTableMeta(*(table.second));
    }
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
    return ;
    //we have a map here
     std::cout<<GREEN_BEGIN<<"PRINT SchemaInfo"<<COLOR_END<<std::endl;
    //only const auto & is allowed, now copying. or we meet use of deleted function.
    for(const auto & child : schema.getChildren()) {
        std::cout<<child.second->getDatabaseID()<<":"<<child.first.getValue()<<std::endl;
        processDatabaseMeta(*(child.second));
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


static void
addToReturn(ReturnMeta *const rm, int pos, const OLK &constr,
            bool has_salt, const std::string &name) {
    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();

    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");

    const int salt_pos = has_salt ? pos + 1 : -1;
    std::pair<int, ReturnField>
        pair(pos, ReturnField(false, name, constr, salt_pos));
    rm->rfmeta.insert(pair);
}

static void
addSaltToReturn(ReturnMeta *const rm, int pos) {

    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();
    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");

    std::pair<int, ReturnField>
        pair(pos, ReturnField(true, "", OLK::invalidOLK(), -1));
    rm->rfmeta.insert(pair);
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
        LOG(cdb_v) << "dec okay";
    }

    assert(out_i && out_i != &i);
    return out_i;
}

static
ResType decryptResults(const ResType &dbres, const ReturnMeta &rmeta) {   
    const unsigned int rows = dbres.rows.size();
    const unsigned int cols = dbres.names.size();
    std::vector<std::string> dec_names;

    for (auto it = dbres.names.begin();
        it != dbres.names.end(); it++) {      
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
                //peel onion.
                dec_rows[r][col_index] =
                    decrypt_item_layers(*dbres.rows[r][c],
                                        fm, rf.getOLK().o, salt);
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

//can not use unique_ptr here in argument 3?
static std::shared_ptr<ReturnMeta> myGetReturnMeta(std::string database, std::string table,\
		SchemaInfo & schema,backupOnionSelection & bonion) {

    std::cout<<"start my decrypt!!"<<std::endl;
    std::shared_ptr<ReturnMeta> myReturnMeta = std::make_shared<ReturnMeta>();
    myReturnMeta->rfmeta.size();

    //construct OLKs for each field!!
    //do not use factory to construct IdentityMetaKey, it's used only upon serial data.
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(database));

    //get databaseMeta
    std::cout<<dbmeta_key->getValue()<<std::endl;
    DatabaseMeta * db = schema.getChild(*dbmeta_key);
    if(db==NULL) {
        std::cout<<"db == NULL"<<std::endl;
	exit(0);
    }else{
        std::cout<<db->getDatabaseID()<<std::endl;
    }

    //get tableMeta
    const std::unique_ptr<IdentityMetaKey> tbMeta_key(new IdentityMetaKey(table));
    TableMeta * tbMeta = (*db).getChild(*tbMeta_key);

    if(tbMeta==NULL){
        std::cout<<"tb == NULL"<<std::endl;
	exit(0);
    }else{
        std::cout<<tbMeta->getDatabaseID()<<std::endl;
    }

    std::cout<<"table anon name: "<<tbMeta->getAnonTableName()<<std::endl;

    //get fieldMeta
    const auto & fields = tbMeta->getChildren();
    //num of fields
    std::cout<<fields.size()<<std::endl;
    //one Olk for each field 

    std::cout<<"fields print: "<<std::endl;

    int pos = 0;
    std::vector<std::string> selectFields;
    //according to uniqueCounter
    for(FieldMeta * field : tbMeta->orderedFieldMetas()) {
        std::cout<<field->getFieldName()<<field->getSaltName()<<std::endl;
        //getOlks!!
        for(std::pair<const OnionMetaKey *, OnionMeta *> oneOnion:field->orderedOnionMetas()){
            std::cout<<oneOnion.first->getValue()<<":"<<oneOnion.second->getAnonOnionName()<<std::endl;
            OLK curOLK(oneOnion.first->getValue(),oneOnion.second->getSecLevel(),field);
            std::cout<<curOLK.o<<std::endl;            
            addToReturn(myReturnMeta.get(),pos++,curOLK,true,field->getFieldName());
            addSaltToReturn(myReturnMeta.get(),pos++);
            selectFields.push_back(oneOnion.second->getAnonOnionName());
            break;
        }
        selectFields.push_back(field->getSaltName());
    }

    auto allFieldMetas = tbMeta->orderedFieldMetas();
    int numOfFields = allFieldMetas.size();
    for(int i=0;i<numOfFields;i++){
        FieldMeta *field = allFieldMetas[i];
	auto allOnionMetas = field->orderedOnionMetas();
	//choose onion and then construct returnmeta,
	//current we choose the first onion
	std::pair<const OnionMetaKey *, OnionMeta *> oneOnion = allOnionMetas[0];
	OLK curOLK(oneOnion.first->getValue(),oneOnion.second->getSecLevel(),field);
	addToReturn(myReturnMeta.get(),pos++,curOLK,true,field->getFieldName());
	addSaltToReturn(myReturnMeta.get(),pos++);
    }
   //we have constructed OLK in myReturnMeta, let's decrypt
//    ResType deResType = decryptResults(backResType,*myReturnMeta);
//    std::cout<<"start parsing deresType!!!!!"<<std::endl;
//    parseResType(deResType); 

    return myReturnMeta;

}

//select query generate, select and retrive the onion selected
static std::string generateSelectQuery(std::string database, std::string table,
		SchemaInfo & schema,backupOnionSelection &bonion) {
    //construct OLKs for each field!!
    //do not use factory to construct IdentityMetaKey, it's used only upon serial data.
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(database));

    //get databaseMeta
    DatabaseMeta * db = schema.getChild(*dbmeta_key);
    if(db==NULL) {
        std::cout<<"db == NULL"<<std::endl;
	return "select NULL";
    }
    //get tableMeta
    const std::unique_ptr<IdentityMetaKey> tbMeta_key(new IdentityMetaKey(table));
    TableMeta * tbMeta = (*db).getChild(*tbMeta_key);

    if(tbMeta==NULL){
        std::cout<<"tb == NULL"<<std::endl;
	return "select NULL";
    }

    //get fieldMeta
    const auto & fields = tbMeta->getChildren();
    //num of fields
    int numOfFields = fields.size();
    assert(numOfFields==bonion.numOfFields);
    //one Olk for each field 
    std::vector<std::string> selectFields;
/*
    //according to uniqueCounter
    for(FieldMeta * field : tbMeta->orderedFieldMetas()) {
        std::cout<<field->getFieldName()<<field->getSaltName()<<std::endl;
        //getOlks!!
        for(std::pair<const OnionMetaKey *, OnionMeta *> oneOnion:field->orderedOnionMetas()){
            selectFields.push_back(oneOnion.second->getAnonOnionName());
            break;
        }
        selectFields.push_back(field->getSaltName());
    }
*/
    std::cout<<"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
    auto allFieldMetas = tbMeta->orderedFieldMetas();
    for(int i=0;i<numOfFields;i++){
    	FieldMeta* field = allFieldMetas[i];
        auto allOnionMetas = field->orderedOnionMetas();
	//for this field, choose an onion
	std::pair<const OnionMetaKey *, OnionMeta *> oneOnion = allOnionMetas[0];
	selectFields.push_back(oneOnion.second->getAnonOnionName());
	selectFields.push_back(field->getSaltName());
    }

    std::string firstFields;
    for(int i=0; i<(int)selectFields.size()-1; i++) {
        firstFields = firstFields + selectFields[i] + ",";
    }
    firstFields += selectFields[selectFields.size()-1];

    //backup(select) only some of the onions
    std::string backQuery = std::string("select "+ firstFields + " from "+database+".") \
			    + std::string(tbMeta->getAnonTableName());
    return backQuery;
}



static void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}



void backupOnionSelection::print() {
   std::cout<<GREEN_BEGIN<<"numOfFields: "<<numOfFields<<COLOR_END<<std::endl;
   std::cout<<"field size: "<<std::endl;

   for(auto item:fieldSize){
       std::cout<<item<<"\t"<<std::endl;
   }
   std::cout<<"saltIndex: "<<std::endl;
   for(auto item:saltIndex){
       std::cout<<item<<"\t"<<std::endl;
   }

   std::cout<<"onionIndex: "<<std::endl;
   for(auto item:onionIndex){
      std::cout<<item<<"\t"<<std::endl;
   }

}

//based on the Metadata, we mordify the original create tabe query and generate the new query.
//static void std::string getCreateTable) {
static std::string getCreateTable(std::string orignalQuery,backupOnionSelection back) {
    std::cout<<"orignalQuery: "<<orignalQuery<<std::endl;
    back.print();
    auto res = split(orignalQuery,',');
    for(auto item:res) {
        std::cout<<item<<std::endl;
    }
    std::string result;
    //choose the salt and onion base on backupOnionSelection.
    for(int i=0;i<back.numOfFields-1;i++){
        int indexOne = back.onionIndex[i];
	int indexTwo = back.saltIndex[i];
	result = result + res[indexOne] +","+ res[indexTwo]+",";
    }
    int indexOne = back.onionIndex[back.numOfFields-1];
    int indexTwo = back.saltIndex[back.numOfFields-1];
    result = result + res[indexOne] + ","+res[indexTwo];

    return result;
}


//generate simple INSERT query for testing purposes.
static 
void generateInsertQuery(rawReturnValue &raw,std::string annoTable) {
    std::cout<<raw.rowValues.size()<<std::endl;
    std::cout<<annoTable<<std::endl;
    int len = raw.fieldNames.size();
    for(auto oneRow:raw.rowValues){
	std::string res= std::string("INSERT INTO ")+annoTable+" VALUES(";
        for(int i=0;i<len-1;i++){
            res = res + oneRow[i]+" , ";
        }
	res = res + oneRow[len-1]+")";
	std::cout<<GREEN_BEGIN<<res<<COLOR_END<<std::endl;
    }
}
static
backupOnionSelection generateBackupStrategy(std::string database, std::string table,
		SchemaInfo & schema){
    //do not use factory to construct IdentityMetaKey, it's used only upon serial data.
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(database));
    //get databaseMeta, search in the map
    DatabaseMeta * db = schema.getChild(*dbmeta_key);
    if(db==NULL) {
        std::cout<<"db == NULL"<<std::endl;
	return backupOnionSelection(0);
    }
    //get tableMeta
    const std::unique_ptr<IdentityMetaKey> tbMeta_key(new IdentityMetaKey(table));
    TableMeta * tbMeta = (*db).getChild(*tbMeta_key);
    if(tbMeta==NULL){
        std::cout<<"tb == NULL"<<std::endl;
	return backupOnionSelection(0);
    }
    //get fieldMeta(we only need size here)
    const auto & fields = tbMeta->getChildren();
    //one Olk for each field
    backupOnionSelection curBack(fields.size());
    int fieldIndex =0;
    //according to uniqueCounter
    for(FieldMeta * field : tbMeta->orderedFieldMetas()) {
	curBack.fieldSize[fieldIndex] = field->getChildren().size()+1;
	curBack.onionIndex[fieldIndex] = 0;
        fieldIndex+=1;
    }
    //complete curBack
    int num = curBack.numOfFields;
    int add=0;
    for(int i=0;i<num;i++){
        int cur = curBack.fieldSize[i];
	curBack.onionIndex[i] = add; 
	add += cur;
        curBack.saltIndex[i] = add-1;
    }
    assert(num=curBack.saltIndex.size());
    assert(num=curBack.onionIndex.size());
    return curBack;
}


static std::string logicBackUp(std::string database, std::string table,SchemaInfo & schema) {
    //do not use factory to construct IdentityMetaKey, it's used only upon serial data.
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(database));
    //get databaseMeta, search in the map
    DatabaseMeta * db = schema.getChild(*dbmeta_key);
    if(db==NULL) {
        std::cout<<"db == NULL"<<std::endl;
	return "";
    }
    //get tableMeta
    const std::unique_ptr<IdentityMetaKey> tbMeta_key(new IdentityMetaKey(table));
    TableMeta * tbMeta = (*db).getChild(*tbMeta_key);
    if(tbMeta==NULL){
        std::cout<<"tb == NULL"<<std::endl;
	return "";
    }
    	//construct OLKs for each field!!
    backupOnionSelection curBack = generateBackupStrategy(database,table,schema);

    std::string logicTableQuery = std::string("SHOW create table "+database+".") +\
				  std::string(tbMeta->getAnonTableName());

    auto res2 = executeAndGetResultRemote(globalConn,logicTableQuery);
    assert(res2.rowValues.size()==1);
    std::vector<std::string> oneRow = res2.rowValues[0];

    //modify and get the create table command.
    getCreateTable(oneRow[1],curBack);

    //then create SELECT command,based on the curBack
    std::string  selectQuery = generateSelectQuery(database,table,schema,curBack);
    auto res3 = executeAndGetResultRemote(globalConn,selectQuery);
    generateInsertQuery(res3, tbMeta->getAnonTableName());    
    return selectQuery;
}

static
void
startBack(){
    return ;
    //only for testing backup module
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
    processSchemaInfo(*schema);
    std::cout<<"please input dbname:####"<<std::endl;
    std::string dbname;
    std::cin>>dbname;
    std::cout<<"please input tableame:####"<<std::endl;
    std::string tablename;
    std::cin>>tablename;
    backupOnionSelection curBack = generateBackupStrategy(dbname,tablename,*schema);
    std::shared_ptr<ReturnMeta> rmeta =  myGetReturnMeta(dbname,tablename,*schema,curBack);
    std::string selectQuery = logicBackUp(dbname,tablename,*schema);

    auto res3 = executeAndGetResultRemote(globalConn,selectQuery);
    const auto dres =  MygetResTypeFromLuaTable(false,&res3);
    if(rmeta.get()!=NULL){
	std::cout<<"decrypted results !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
        ResType deResType = decryptResults(dres,*rmeta); 
	parseResType(deResType);
	std::cout<<"decrypted results !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<std::endl;
    }
}



int
main() {
   
 

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
    std::getline(std::cin,curQuery);
    unsigned long long _thread_id = globalConn->get_thread_id();
    while(curQuery!="quit"){
        if(curQuery.size()==0){
            std::cout<<std::endl;
            std::getline(std::cin,curQuery);            
            continue;
        }
        if(curQuery=="back"){
            startBack();
            std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
            processSchemaInfo(*schema);
        }else{	
            std::cout<<GREEN_BEGIN<<"curQuery: "<<curQuery<<"\n"<<COLOR_END<<std::endl;
            batchTogether(client,curQuery,_thread_id);
        }
        std::cout<<GREEN_BEGIN<<"\nplease input a new query:#######"<<COLOR_END<<std::endl;
        std::getline(std::cin,curQuery);
    }


    return 0;
}
