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

char * globalEsp=NULL;

int num_of_pipe = 4;

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
    std::vector<int> maxlengths;/*what's the difference between length and maxlength?*/
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


//helper function for transforming the rawMySQLReturnValue

static Item_null *
make_null(const std::string &name = ""){
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}
//helper function for transforming the rawMySQLReturnValue
static std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null());
    }
    return out;
}

//transform rawMySQLReturnValue to ResType
static 
ResType MygetResTypeFromLuaTable(bool isNULL,rawMySQLReturnValue *inRow = NULL,int in_last_insert_id = 0){
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
//        uint64_t afrow = globalConn->get_affected_rows();
//	std::cout<<GREEN_BEGIN<<"Affected rows: "<<afrow<<COLOR_END<<std::endl;
        return ResType(true, 0 ,
                               in_last_insert_id, std::move(names),
                                   std::move(types), std::move(rows));
    }
}


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
    std::vector<OnionMeta*>originalOm;
    void show(){
        for(auto i=0U;i<fields.size();i++){
             //cout<<fields[i]<<" : "<<gmp2[onions[i]]<<"\t";
        }
        cout<<endl;
        if(hasSalt){
            cout<<"has salt"<<endl;
        }else cout<<"do not have salt"<<endl;
    }
};

/*for each field, convert the format to transField*/
static std::vector<transField> getTransField(std::vector<FieldMeta *> pfms){
    std::vector<transField> res;
    //for every field
    for(auto pfm:pfms){
        transField tf;
	    tf.originalFm = pfm;
        for(std::pair<const OnionMetaKey *, OnionMeta *> &ompair:pfm->orderedOnionMetas()){
            tf.numOfOnions++;
            tf.fields.push_back((ompair.second)->getAnonOnionName());
            tf.onions.push_back(ompair.first->getValue());
            tf.originalOm.push_back(ompair.second);
        }
        if(pfm->getHasSalt()){
            tf.hasSalt=true;
	        tf.fields.push_back(pfm->getSaltName());
        }
        res.push_back(tf);
    }
    return res;
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

/*
structure of return field. 
map<int,returnField>, int is the index of names
returnField, represent a field, if the field is not salt, then fieldCalled is the plaintex name
*/
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

    std::vector<enum_field_types> types;

    for(auto item:dec_rows[0]){
        types.push_back(item->field_type());
    }

    //resType is used befor and after descrypting.
    return ResType(dbres.ok, dbres.affected_rows, dbres.insert_id,
                   std::move(dec_names),
                   std::vector<enum_field_types>(types),//different from previous version
                   std::move(dec_rows));
}

//get returnMeta
//for each filed, we have a fieldmeta. we can chosse one onion under that field to construct a return meta.
//in fact, a returnmeta can contain many fields.
static
std::shared_ptr<ReturnMeta> getReturnMeta(std::vector<FieldMeta*> fms, std::vector<transField> &tfds){
    assert(fms.size()==tfds.size());
    std::shared_ptr<ReturnMeta> myReturnMeta = std::make_shared<ReturnMeta>();
    int pos=0;
    //construct OLK
    for(auto i=0u;i<tfds.size();i++){
        OLK curOLK(tfds[i].onions[tfds[i].onionIndex],
                tfds[i].originalOm[tfds[i].onionIndex]->getSecLevel(),tfds[i].originalFm);
	addToReturn(myReturnMeta.get(),pos++,curOLK,true,tfds[i].originalFm->getFieldName());
        addSaltToReturn(myReturnMeta.get(),pos++);
    }
    return myReturnMeta;
}

struct meta_file{
    string db,table;
    int num_of_fields;
    vector<string> field_types;
    vector<int> field_lengths;
    vector<string> field_names;
    vector<int> choosen_onions;

    void show(){
        cout<<db<<endl;
        cout<<table<<endl;
        cout<<num_of_fields<<endl;
        for(auto item:field_types){
            cout<<item<<"\t";
        }
        cout<<endl;
        for(auto item:field_lengths){
            cout<<item<<"\t";
        }

	cout<<endl;
        for(auto item:field_names){
            cout<<item<<"\t";
        }
	cout<<endl;
        for(auto item:choosen_onions){
           cout<<item<<"\t";
        }
        cout<<endl;
    }
};

static meta_file load_meta(string db="tdb", string table="student", string filename="metadata.data"){
    //FILE * meta = NULL;
    //localmeta = fopen(filename.c_str(),"r");
    filename = string("data/")+db+"/"+table+"/"+filename;
    std::ifstream infile(filename);
    string line;
    meta_file res;
    while(std::getline(infile,line)){
        int index = line.find(":");
        string head = line.substr(0,index);
        if(head=="database"){
            res.db = line.substr(index+1);
        }else if(head=="table"){
            res.table = line.substr(index+1);
        }else if(head=="num_of_fields"){
            res.num_of_fields = std::stoi(line.substr(index+1));
        }else if(head=="field_types"){
            string types = line.substr(index+1);
            int start=0,next=0;
            while((next=types.find(' ',start))!=-1){
                string item = types.substr(start,next-start);
                res.field_types.push_back(item);
                start = next+1;
            }
            string item = types.substr(start);
            res.field_types.push_back(item);
        }else if(head=="field_lengths"){
            string lengths = line.substr(index+1);
            int start=0,next=0;
            while((next=lengths.find(' ',start))!=-1){
                string item = lengths.substr(start,next-start);
                res.field_lengths.push_back(std::stoi(item));
                start = next+1;
            }
            string item = lengths.substr(start);
            res.field_lengths.push_back(std::stoi(item));
        }else if(head=="field_names"){
            string names = line.substr(index+1);
            int start=0,next=0;
            while((next=names.find(' ',start))!=-1){
                string item = names.substr(start,next-start);
                res.field_names.push_back(item);
                start = next+1;
            }
            string item = names.substr(start);
            res.field_names.push_back(item);
        }else if(head=="choosen_onions"){
            string c_onions = line.substr(index+1);
            int start=0,next=0;
            while((next=c_onions.find(' ',start))!=-1){
                string item = c_onions.substr(start,next-start);
                res.choosen_onions.push_back(std::stoi(item));
                start = next+1;
            }
            string item = c_onions.substr(start);
            res.choosen_onions.push_back(std::stoi(item));
        }
    }
    return res;
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

static vector<vector<string>> load_table_fields(meta_file & input) {
    string db = input.db;
    string table = input.table;
    vector<vector<string>> res;
    string prefix = string("data/")+db+"/"+table+"/";

    vector<string> datafiles;
    for(auto item:input.field_names){
        datafiles.push_back(prefix+item);
    }

    for(unsigned int i=0u;i<input.field_names.size();i++){
       vector<string> column;
       if(IS_NUM(std::stoi(input.field_types[i]))){
           load_num(datafiles[i],column);
       }else{
           load_string(datafiles[i],column,input.field_lengths[i]);
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
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
    //get all the fields in the tables.
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);
    auto res = getTransField(fms);

    std::vector<enum_field_types> types;//Added
    for(auto item:fms){
        types.push_back(item->getSqlType());
    }//Add new field form FieldMeta
    if(types.size()==1){
        //to be
    }

    meta_file res_meta = load_meta(db,table);

    for(unsigned int i=0;i<res_meta.choosen_onions.size();i++){
	res[i].choosenOnions.push_back(res_meta.choosen_onions[i]);
    }
    std::shared_ptr<ReturnMeta> rm = getReturnMeta(fms,res);

    //why do we need this??
    std::string backq = "show databases";
    executeAndGetResultRemote(globalConn,backq);
    rawMySQLReturnValue resraw2;
    //load fields in the stored file
    vector<vector<string>> res_field = load_table_fields(res_meta);
    resraw2.rowValues = res_field;
    resraw2.fieldNames = res_meta.field_names;
    resraw2.choosen_onions = res_meta.choosen_onions;
    for(unsigned int i=0;i<res_meta.field_types.size();++i) {
	resraw2.fieldTypes.push_back(static_cast<enum_field_types>(std::stoi(res_meta.field_types[i])));
    }
    ResType rawtorestype = MygetResTypeFromLuaTable(false, &resraw2);
    auto finalresults = decryptResults(rawtorestype,*rm);
//    parseResType(finalresults);

    return finalresults;
}

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



static void add(rawMySQLReturnValue & str,ResType & item ){
    for(auto row : item.rows){
        std::vector<string> temp;
        for(auto item : row){
            temp.push_back(ItemToString(*item));
        }
        str.rowValues.push_back(temp);
    }
    str.fieldTypes = item.types;
}


static void construct_insert(rawMySQLReturnValue & str,std::string table,std::vector<string> &res){    
    std::string head = string("INSERT INTO `")+table+"` VALUES ";

    int cnt = 0;
    string cur=head;
    for(unsigned int i=0u; i<str.rowValues.size();i++){
        ++cnt;
        cur+="(";        
        for(unsigned int j=0u;j<str.rowValues[i].size();j++){
            if(IS_NUM(str.fieldTypes[j])) {
//                cout<<str.fieldTypes[j]<<endl;
                cur+=str.rowValues[i][j]+=",";
//                cout<<"isnum"<<endl;
            }else{
                //cur+=string("\"")+=str.rowValues[i][j]+="\",";
                int len = str.rowValues[i][j].size();
                mysql_real_escape_string(globalConn->get_conn(),globalEsp,
                    str.rowValues[i][j].c_str(),len);
                cur+=string("\"")+=string(globalEsp)+="\",";
            }
        }
        cur.back()=')';
        cur+=",";
        if(cnt == num_of_pipe){
            cnt = 0;
            cur.back()=';';
            res.push_back(cur);
            cur=head;
        }
    }
    if(cnt!=0){
        cur.back()=';';
        res.push_back(cur);
    }
}

int
main(int argc, char* argv[]) {
    init();
    std::string db="tdb",table="student";
    globalEsp = (char*)malloc(sizeof(char)*5000);
    if(globalEsp==NULL){
        printf("unable to allocate esp\n");
        return 0;
    }
    ResType res =  load_files(db,table);
    rawMySQLReturnValue str;
    add(str,res);
    std::vector<string> res_query;
    construct_insert(str,table,res_query);
    for(auto item:res_query){
        cout<<item<<endl;
    }    
    free(globalEsp);
    return 0;
}
