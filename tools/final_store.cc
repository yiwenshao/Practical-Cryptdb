#include "wrapper/common.hh"
#include "wrapper/reuse.hh"
//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;

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
        //actually the salt should be selected if RND is used,this should be changed later.
    	if(tf.getHasSalt()){
            res += tf.getSaltName() + " , ";
        }
    }
    res = res.substr(0,res.size()-2);
    res = res + "FROM `"+db+std::string("`.`")+annotablename+"`";
    return res;
}

static void write_meta(std::vector<FieldMetaTrans> &res,string db,string table){
    TableMetaTrans mf(db,table,res);
    mf.set_db_table(db,table);
    mf.serialize();
}

static
void write_raw_data_to_files(MySQLColumnData& resraw,std::vector<FieldMetaTrans> &res ,string db,string table){
    //write metafiles
    write_meta(res,db,table);
    //write datafiles
    std::string prefix = std::string("data/") +db+"/"+table+"/";
    std::vector<std::string> filenames;
    for(auto item:resraw.fieldNames){
        item=prefix+item;
        filenames.push_back(item);
    }
    int len = resraw.fieldNames.size();

    for(int i=0;i<len;i++){
        if(IS_NUM(resraw.fieldTypes[i])){
            writeColumndataNum(resraw.columnData[i],filenames[i]);
        }else{
            writeColumndataEscapeString(resraw.columnData[i],filenames[i],resraw.maxLengths[i]);
        }
    }
}

static void store(std::string db, std::string table){
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(gembeddedDir);
    //get all the fields in the tables
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);

    //transform the field so that selected onions can be used
    std::vector<FieldMetaTrans> res;
    for(auto i=0u;i<fms.size();i++){
        FieldMetaTrans ft;
        res.push_back(ft);
        res.back().trans(fms[i]);
    }
    /*this is our strategy, each field should be able to choose the selected onion*/
    storeStrategies(res);
    //generate the backup query and then fetch the tuples
    std::string backup_query = getTestQuery(*schema,res,db,table);
    MySQLColumnData resraw =  executeAndGetColumnData(globalConn,backup_query);
    //then we should set the type and length of FieldMetaTrans
    auto types = resraw.fieldTypes;
    auto lengths = resraw.maxLengths;
    int base_types = 0;
    int base_lengths = 0;
    for(auto &item:res){
        vector<int> tempTypes;
        vector<int> tempLengths;
        for(unsigned int i=0u;i<item.getChoosenOnionName().size();i++){
            tempTypes.push_back(types[base_types++]);
            tempLengths.push_back(lengths[base_lengths++]);
        }
        item.setChoosenFieldTypes(tempTypes);
        item.setChoosenFieldLengths(tempLengths);
        if(item.getHasSalt()){//also this should be changed.
            item.setSaltType(types[base_types++]);
            item.setSaltLength(lengths[base_lengths++]);
        }
    }
    //write the tuples into files
    write_raw_data_to_files(resraw,res,db,table);
}

int
main(int argc, char* argv[]){    
    std::string db="tdb",table="student";
    std::string ip="127.0.0.1";
    int port=3306;
    if(argc==4){
        ip = std::string(argv[1]);
        db = std::string(argv[2]);
        table = std::string(argv[3]);
    }
    globalConn = globalInit(ip,port);
    store(db,table);
    return 0;
}
