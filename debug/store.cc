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


//query for testing purposes
static
std::string getTestQuery(SchemaInfo &schema, std::vector<FieldMetaTrans> &tfds,
                                     std::string db="tdb",std::string table="student1"){
    std::string res = "SELECT ";
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(db));
    //get databaseMeta, search in the map
    DatabaseMeta * dbm = schema.getChild(*dbmeta_key);
    const TableMeta & tbm = *((*dbm).getChild(IdentityMetaKey(table)));
    std::string annotablename = tbm.getAnonTableName();

    //then a list of onion names
    for(auto tf:tfds){
        for(auto item : tf.getChoosenOnionName()){
            res += item;
            res += " , ";
        }
    	if(tf.getHasSalt()){
            res += tf.getSaltName() + " , ";
        }
    }
    res = res.substr(0,res.size()-2);
    res = res + "FROM `"+db+std::string("`.`")+annotablename+"`";
    return res;
}

static void write_meta(rawMySQLReturnValue& resraw,std::vector<FieldMetaTrans> &res,string db,string table){
    metadata_files mf;
    mf.set_db_table(db,table);
    vector<vector<int>> selected_field_types;
    vector<vector<int>> selected_field_lengths;
    vector<vector<string>> selected_field_names;
    vector<vector<int>> selected_onion_index;
    vector<int> dec_onion_index;
    vector<string> has_salt;

    unsigned int type_index=0u,length_index=0u;

    for(auto ft:res){
        vector<int> field_types;
        vector<int> field_lengths;
        
        vector<string> field_names;
        //only choosen fields
        for(auto item:ft.getChoosenOnionName()){
            field_names.push_back(item);
        }
        if(ft.getHasSalt()){
            field_names.push_back(ft.getSaltName());
        }

        int onion_index = 0;

        for(unsigned int i=0u;i<field_names.size();i++){
            field_types.push_back(static_cast<int>(resraw.fieldTypes[type_index]));
            type_index++;
        }
        for(unsigned int i=0u;i<field_names.size();i++){
            field_lengths.push_back(resraw.lengths[length_index]);
            length_index++;
        }

        if(ft.getHasSalt()){
            has_salt.push_back("true");
        }else has_salt.push_back("false");

        selected_field_types.push_back(field_types);
        selected_field_lengths.push_back(field_lengths);
        selected_field_names.push_back(field_names);
        dec_onion_index.push_back(onion_index);
    }
    mf.set_selected_field_types(selected_field_types);
    mf.set_selected_field_lengths(selected_field_lengths);
    mf.set_selected_field_names(selected_field_names);
    mf.set_dec_onion_index(dec_onion_index);
    mf.set_has_salt(has_salt);
    mf.serialize();
}

static
void write_raw_data_to_files(rawMySQLReturnValue& resraw,std::vector<FieldMetaTrans> &res ,string db,string table){
    //write metafiles
    write_meta(resraw,res,db,table);
    //write datafiles
    write_row_data(resraw,db,table);
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
        //this is our strategy !!!!!
        res.back().choose(in);
    }
    //generate the backup query and then fetch the tuples
    std::string backup_query = getTestQuery(*schema,res,db,table);
    rawMySQLReturnValue resraw =  executeAndGetResultRemote(globalConn,backup_query);
    //write the tuples into files
    write_raw_data_to_files(resraw,res,db,table);
}

int
main(int argc, char* argv[]){
    init();
    std::string db="tdb",table="student";
    store(db,table);
    return 0;
}
