#include "big_proxy.hh"

using std::cout;
using std::cin;
using std::endl;
using std::string;

//global map, for each client, we have one WrapperState which contains ProxyState.

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
	//std::cout<<GREEN_BEGIN<<"Affected rows: "<<afrow<<COLOR_END<<std::endl;
        return ResType(true, 0 ,
                               in_last_insert_id, std::move(names),
                                   std::move(types), std::move(rows));
    }
}

//printResType for testing purposes
static 
void parseResType(const ResType &rd) {
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

big_proxy::big_proxy(std::string db,std::string ip,std::string user,std::string passwd,int port){
        client="192.168.1.1:1234";
        //one Wrapper per user.
        clients[client] = new WrapperState();    
        //Connect phase
        ConnectionInfo ci(ip, user, passwd,port);
        //const std::string master_key = "113341234";
        const std::string master_key = "113341234";
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
        targetDb=db;
        curQuery = string("use ")+targetDb;
        _thread_id = globalConn->get_thread_id();
}


void big_proxy::myNext(std::string client,bool isFirst,ResType inRes) {
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
        //    std::cout<<RED_BEGIN<<"case one"<<COLOR_END<<std::endl;
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
            const auto &new_query =
                std::get<1>(new_results)->extract<std::string>();
            auto resRemote = executeAndGetResultRemote(globalConn,new_query);
            printrawReturnValue(resRemote);
            break;
        }

        //return the results to the client directly 
        case AbstractQueryExecutor::ResultType::RESULTS:{
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


void big_proxy::batchTogether(std::string client, std::string curQuery,unsigned long long _thread_id) {
    //the first step is to Rewrite, we abort this session if we fail here.
    bool resMyRewrite =  myRewrite(curQuery,_thread_id,client);
    if(!resMyRewrite){
         return ; 
    }
    myNext(client,true, MygetResTypeFromLuaTable(true));
}


bool big_proxy::myRewrite(std::string curQuery,unsigned long long _thread_id,std::string client) {
    assert(0 == mysql_thread_init());
    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = c_wrapper->ps.get();
    assert(ps);
    c_wrapper->last_query = curQuery;
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

void big_proxy::go(std::string query){
        batchTogether(client,query,_thread_id);
}


