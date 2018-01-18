#include "wrapper/common.hh"
#include "wrapper/reuse.hh"
static std::string embeddedDir="/t/cryt/shadow";
//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;
//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;

//must be static, or we get "no previous declaration"
//execute the query and get the rawReturnVale, this struct can be copied.


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



static void write_meta(std::vector<FieldMetaTrans> &res,string db,string table){
    vector<int> in{1,2,3,4};
    for(auto &item:res){
        item.setChoosenFieldLengths(in);
        item.setChoosenFieldTypes(in);
    }
    TableMetaTrans mf(db,table,res);
    mf.set_db_table(db,table);

    mf.serialize();

    TableMetaTrans mf2;
    mf2.set_db_table(db,table);
    mf2.deserialize();

    return;

}

static
void write_raw_data_to_files(std::vector<FieldMetaTrans> &res ,string db,string table){
    //write metafiles
    write_meta(res,db,table);
}

static void store(std::string db, std::string table){
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(embeddedDir);
    //get all the fields in the tables
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);
    //transform the field so that selected onions can be used
    std::vector<FieldMetaTrans> res;
    for(auto i=0u;i<fms.size();i++){
        FieldMetaTrans ft;
        res.push_back(ft);
        res.back().trans(fms[i]);
        std::vector<int> in{0};
        res.back().choose(in);
    }
    write_raw_data_to_files(res,db,table);
}

int
main(int argc, char* argv[]){
    init();
    std::string db="tdb",table="student";
    store(db,table);
    return 0;
}
