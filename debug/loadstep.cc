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

struct batch{
    vector<string> field_names;
    vector<int> field_types;
    vector<int> field_lengths;
};

batch ggbt;

/*should choose the right decryption onion*/
static
std::shared_ptr<ReturnMeta> getReturnMeta(std::vector<FieldMeta*> fms,
                                      std::vector<FieldMetaTrans> &tfds){
    assert(fms.size()==tfds.size());
    std::shared_ptr<ReturnMeta> myReturnMeta = std::make_shared<ReturnMeta>();
    int pos=0;
    //construct OLK
    for(auto i=0u;i<tfds.size();i++){
        int index = getDecryptionOnionIndex(tfds[i]);
        onion o = tfds[i].getChoosenOnionO()[index];
        SECLEVEL l = tfds[i].getOriginalFieldMeta()->getOnionMeta(o)->getSecLevel();
        FieldMeta *k = tfds[i].getOriginalFieldMeta();
        OLK curOLK(o,l,k);
	addToReturn(myReturnMeta.get(),pos++,curOLK,true,k->getFieldName());
        addSaltToReturn(myReturnMeta.get(),pos++);

        ggbt.field_types.push_back(tfds[i].getChoosenFieldTypes()[index]);
        ggbt.field_names.push_back(tfds[i].getChoosenOnionName()[index]);
        ggbt.field_lengths.push_back(tfds[i].getChoosenFieldLengths()[index]);        
        ggbt.field_types.push_back(tfds[i].getSaltType());
        ggbt.field_names.push_back(tfds[i].getSaltName());
        ggbt.field_lengths.push_back(tfds[i].getSaltLength());
    }
    return myReturnMeta;
}

static TableMetaTrans load_meta(string db="tdb", string table="student", string filename="metadata.data"){
    TableMetaTrans mf;
    mf.set_db_table(db,table);
    mf.deserialize();
    return mf;
}

static void load_num(string filename,vector<string> &res){
    std::ifstream infile(filename);
    string line;
    while(std::getline(infile,line)){
        res.push_back(line);
    }
    infile.close();
}

static void load_string(string filename, vector<string> &res,unsigned long length){
    char *buf = new char[length];
    int fd = open(filename.c_str(),O_RDONLY);
    while(read(fd,buf,length)!=0){
        res.push_back(string(buf,length));
    }
    close(fd);
}

/*load fields in string, only part of it can be decrypted*/
static vector<vector<string>> load_table_fields(TableMetaTrans & input,
                                    std::vector<FieldMetaTrans> &tfms) {
    string db = input.get_db();
    string table = input.get_table();
    vector<vector<string>> res;
    string prefix = string("data/")+db+"/"+table+"/";

    vector<string> field_names = ggbt.field_names;
    vector<int> field_types = ggbt.field_types;
    vector<int> field_lengths = ggbt.field_lengths;
 
    vector<string> datafiles;  
    for(auto item:field_names){
        datafiles.push_back(prefix+item);
    }

    for(unsigned int i=0u;i<field_names.size();i++){
       vector<string> column;
       if(IS_NUM(field_types[i])){
           load_num(datafiles[i],column);
       }else{
           load_string(datafiles[i],column,field_lengths[i]);
       }
       for(unsigned int j=0u; j<column.size(); j++){
           if(j>=res.size()){
               res.push_back(vector<string>());
           }
           res[j].push_back(column[j]);
       }
    }
    return res;
}

static ResType load_files(std::string db="tdb", std::string table="student"){
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(embeddedDir);
    //get all the fields in the tables.
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);
    TableMetaTrans res_meta = load_meta(db,table);
    std::vector<FieldMetaTrans> res2 = res_meta.getFts();
    for(unsigned int i=0;i<fms.size();i++){
        res2[i].trans(fms[i]);
    }
    std::shared_ptr<ReturnMeta> rm = getReturnMeta(fms,res2);
    //why do we need this??
    create_embedded_thd(0);
    rawMySQLReturnValue resraw;
    //load fields in the stored file
    vector<vector<string>> res_field = load_table_fields(res_meta,res2);

    resraw.rowValues = res_field;

    vector<string> field_names = ggbt.field_names;
    vector<int> field_types = ggbt.field_types;
    vector<int> field_lengths = ggbt.field_lengths;

    resraw.fieldNames = field_names;
    for(unsigned int i=0;i<field_types.size();++i){
	resraw.fieldTypes.push_back(static_cast<enum_field_types>(field_types[i]));
    }
    ResType rawtorestype = rawMySQLReturnValue_to_ResType(false, &resraw);
    auto finalresults = decryptResults(rawtorestype,*rm);
    return finalresults;
}


static std::ostream&
insert_list_show(std::ostream &out,List<List_item> &newList){
    out << " VALUES " << noparen(newList)<<";";
    return out;
}


static
void local_wrapper(const Item &i, const FieldMeta &fm, Analysis &a,
                           List<Item> *const append_list){
    //为什么这里不是push item??
//    append_list->push_back(&(const_cast<Item&>(i)));
    //do not use the plain strategy 

    std::vector<Item *> l;
    my_typical_rewrite_insert_type(i,fm,a,&l);
    for (auto it : l) {
        append_list->push_back(it);
    }
}


int
main(int argc, char* argv[]){
    init();
    create_embedded_thd(0);
    std::string db="tdb",table="student";

    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(embeddedDir);
    schema.get();
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    Analysis analysis(db, *schema, TK, SECURITY_RATING::SENSITIVE);

    /*choose decryption onion, load and decrypt to plain text*/
    ResType res =  load_files(db,table);
    std::string annoTableName = analysis.getTableMeta(db,table).getAnonTableName();

    const std::string head = std::string("INSERT INTO `")+db+"`.`"+annoTableName+"` ";

    /*reencryption to get the encrypted insert!!!*/
    for(auto &row:res.rows){
        List<List_item> newList;
        List<Item> *const newList0 = new List<Item>();
        for(auto i=0u;i<res.names.size();i++){
            std::string field_name = res.names[i];
            FieldMeta & fm = analysis.getFieldMeta(db,table,field_name);
            local_wrapper(*row[i],fm,analysis,newList0);
        }
        newList.push_back(newList0);
        std::ostringstream o;
        insert_list_show(o,newList);
        std::cout<<(head+o.str())<<std::endl;
    }
    return 0;
}

