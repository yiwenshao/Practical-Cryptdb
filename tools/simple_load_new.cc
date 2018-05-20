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
#include <algorithm>
#include "wrapper/reuse.hh"
#include "wrapper/common.hh"
#include "wrapper/insert_lib.hh"
#include "util/constants.hh"
#include "util/timer.hh"
#include "util/log.hh"
#include "util/util.hh"

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;
static std::string embeddedDir="/t/cryt/shadow";
char * globalEsp=NULL;
int num_of_pipe = 4;
//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;

static
std::string logfilePrefix = "final_load";

static
std::string logfileName = logfilePrefix+constGlobalConstants.logFile+std::to_string(time(NULL));

logToFile *glog;

//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;
/*for each field, convert the format to FieldMeta_Wrapper*/
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

//========================================================================================//

//global full backup
fullBackUp gfb;

static
void
load_columns(std::vector<FieldMetaTrans> &fmts,std::string db,std::string table){
    for(auto &item:fmts){
        std::string prefix = std::string("data/")+db+"/"+table+"/"
                             + item.getOriginalFieldMeta()->getFieldName()+"/";
        //load onions for each field
        for(unsigned int i=0u;i<item.getChoosenOnionName().size();i++){
            std::string filename = prefix+item.getChoosenOnionName()[i];
            std::vector<std::string> column;
            if(IS_NUM(item.getChoosenFieldTypes()[i])){
                loadFileNoEscapeLimitCount(filename,column,constGlobalConstants.loadCount);
            }else{
                loadFileEscapeLimitCount(filename,column,
                                         item.getChoosenFieldLengths()[i],
                                         constGlobalConstants.loadCount);
            }
            gfb.annoOnionNameToFileVector[item.getChoosenOnionName()[i]]=std::move(column);
        }  
        if(item.getHasSalt()) {
            std::vector<std::string> column;
            std::string filename = prefix + item.getSaltName();
            loadFileNoEscapeLimitCount(filename,column,constGlobalConstants.loadCount);
            gfb.annoOnionNameToFileVector[item.getSaltName()] = std::move(column);
        }
    }
}

static ResType load_files_new(std::string db, std::string table){
    timer t_load_files;

    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(embeddedDir);
    //get all the fields in the tables.
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);
    TableMetaTrans mf;
    mf.set_db_table(db,table);
    mf.deserializeNew();

    std::vector<FieldMetaTrans>& fmts = mf.getFtsRef();
    for(unsigned int i=0;i<fms.size();i++){
        fmts[i].trans(fms[i]);
    }
    mf.show();
    load_columns(fmts,db,table);
//    initGfb(fmts,db,table);//load global columns
/* 
    std::shared_ptr<ReturnMeta> rm = getReturnMeta(fms,res);

    *glog<<"getReturnMeta: "<<
          std::to_string(t_load_files.lap()/1000000u)<<
          "##"<<std::to_string(time(NULL))<<"\n";

    vector<string> field_names = ggbt.field_names;
    vector<int> field_types = ggbt.field_types;
    vector<int> field_lengths = ggbt.field_lengths;

    //why do we need this??
    create_embedded_thd(0);
    rawMySQLReturnValue resraw;
    vector<vector<string>> res_field;   
    for(auto item:field_names){
        res_field.push_back(gfb.annoOnionNameToFileVector[item]);
    }
    //check here
    for(auto item:res_field){
        if(item.size()==0) {
            return ResType(false, 0, 0);
        }
    }


    //then transform it to ress_fields
    unsigned int length = res_field[0].size();

    vector<vector<string>> ress_field;
    for(unsigned int i=0u;i<length;i++){
        vector<string> row;
        for(unsigned int j=0u;j<res_field.size();j++){
            row.push_back(res_field[j][i]);
        }
        ress_field.push_back(row);
    }

    resraw.rowValues = ress_field;
    resraw.fieldNames = field_names;
    for(unsigned int i=0;i<field_types.size();++i){
	resraw.fieldTypes.push_back(static_cast<enum_field_types>(field_types[i]));
    }
    ResType rawtorestype = rawMySQLReturnValue_to_ResType(false, &resraw);

    *glog<<"transform: "<<
          std::to_string(t_load_files.lap()/1000000u)<<
          "##"<<std::to_string(time(NULL))<<"\n";

    auto finalresults = decryptResults(rawtorestype,*rm);

    *glog<<"descryption: "<<
           std::to_string(t_load_files.lap()/1000000u)<<
           "##"<<std::to_string(time(NULL))<<"\n";
    return finalresults;
*/
    return ResType(false, 0, 0);   
}




std::map<onion,unsigned long> gcountMap;

static
void local_wrapper(const Item &i, const FieldMeta &fm, Analysis &a,
                           List<Item> * append_list) {
    //append_list->push_back(&(const_cast<Item&>(i)));
    //do not use the plain strategy 
    std::vector<Item *> l;
    const uint64_t salt = fm.getHasSalt() ? randomValue() : 0;
    uint64_t IV = salt;
    for (auto it : fm.orderedOnionMetas()) {
        if(RiboldMYSQL::is_null(i)){
            l.push_back(RiboldMYSQL::clone_item(i));
            continue;
        }
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;
        std::string annoOnionName = om->getAnonOnionName();
        if(gfb.annoOnionNameToFileVector.find(annoOnionName)!=gfb.annoOnionNameToFileVector.end()){
            enum_field_types type = static_cast<enum_field_types>(gfb.annoOnionNameToType[annoOnionName]);
            std::vector<std::string> &tempFileVector = gfb.annoOnionNameToFileVector[annoOnionName];
            std::string in = tempFileVector.back();            
            if(IS_NUM(type)){
                unsigned int len = annoOnionName.size();
                if(len>4u&&annoOnionName.substr(len-4)=="ASHE"){
                    l.push_back(MySQLFieldTypeToItem(type,in));
                }else{
                    l.push_back( new (current_thd->mem_root)
                                Item_int(static_cast<ulonglong>(valFromStr(in))) );
                }
            }else{
                l.push_back(MySQLFieldTypeToItem(type,in));
            }
            tempFileVector.pop_back();
        }else{
            l.push_back(my_encrypt_item_layers(i, o, *om, a, IV));
            gcountMap[o]++;
        }
    }
    std::string saltName = fm.getSaltName();
    if (fm.getHasSalt()) {
        if(gfb.annoOnionNameToFileVector.find(saltName)!=gfb.annoOnionNameToFileVector.end()){
            std::vector<std::string> &tempFileVector = gfb.annoOnionNameToFileVector[saltName];
            std::string in = tempFileVector.back();
            l.push_back( new (current_thd->mem_root)
                                Item_int(static_cast<ulonglong>(valFromStr(in)))
             );
            tempFileVector.pop_back();
            gcountMap[oINVALID]++;//use invalid to record the salt hit rate
        }else{
            l.push_back(new Item_int(static_cast<ulonglong>(salt)));
        }
    }

    for (auto it : l) {
        append_list->push_back(it);
    }
}

static
List<Item> * processRow(const std::vector<Item *> &row,
                        const std::vector<std::string> &names,
                        Analysis &analysis,
                        std::string db,
                        std::string table) {
    List<Item> *const newList0 = new List<Item>();
    for(auto i=0u;i<names.size();i++){
        std::string field_name = names[i];
        FieldMeta & fm = analysis.getFieldMeta(db,table,field_name);
        local_wrapper(*row[i],fm,analysis,newList0);
    }
    return newList0;
}


int
main(int argc, char* argv[]){
    timer t_init;
    init();
    create_embedded_thd(0);
    std::string db="tdb",table="student";
    std::string ip="localhost";
    if(argc==4){
        ip = std::string(argv[1]);
        db = std::string(argv[2]);
        table = std::string(argv[3]);
    }
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(embeddedDir);
    schema.get();
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    Analysis analysis(db, *schema, TK, SECURITY_RATING::SENSITIVE);
    logToFile ll(table+logfileName);
    glog = &ll;
    *glog<<"loadSchema: "<<
          std::to_string(t_init.lap()/1000000u)<<
          "##"<<std::to_string(time(NULL))<<"\n";
    ResType res =  load_files_new(db,table);
    if(!res.success()){
        *glog<<"empty table \n";
        return 0;
    }

/*
    *glog<<"load_files: "<<
          std::to_string(t_init.lap()/1000000u)<<
          "##"<<std::to_string(time(NULL))<<"\n";

    std::string annoTableName = analysis.getTableMeta(db,table).getAnonTableName();
    const std::string head = std::string("INSERT INTO `")+db+"`.`"+annoTableName+"` ";


    unsigned int i=0u;
    while(true){
        List<List_item> newList;
        int localCount=0;
        for(;i<res.rows.size();i++){
            List<Item> * newList0 = processRow(res.rows[i],
                                               res.names,
                                               analysis,db,table);
            newList.push_back(newList0);
            localCount++;
            if(localCount==constGlobalConstants.pipelineCount){
                std::ostringstream o;
                insertManyValues(o,newList);
                std::cout<<(head+o.str())<<std::endl;
                i++;
                break;
            }
        }
        if(i>=res.rows.size()){
            if(localCount!=constGlobalConstants.pipelineCount) {
                std::ostringstream o;
                insertManyValues(o,newList);
                std::cout<<(head+o.str())<<std::endl;
            }
            break;
        }
    }
    *glog<<"reencryptionAndInsert: "<<
        std::to_string(t_init.lap()/1000000u)<<
        "##"<<std::to_string(time(NULL))<<"\n";
    for(auto item:gcountMap) {
        *glog<<"onionComputed: "<<
              TypeText<onion>::toText(item.first)<<"::"<<
              std::to_string(item.second)<<"\n";
    }*/
    UNUSED(load_files_new);
    UNUSED(processRow);
    return 0;
}

