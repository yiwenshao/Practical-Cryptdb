#include "debug/load.hh"
#include "wrapper/common.hh"

static
Item_null *
make_null(const std::string &name){
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}


static
std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out; 
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null(""));
    }
    return out;
}



static
ResType MygetResTypeFromLuaTable(bool isNULL,rawMySQLReturnValue *inRow = NULL,int in_last_insert_id = 0){
    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    std::vector<std::vector<Item *>> rows;
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
        return ResType(true,0,in_last_insert_id,std::move(names),std::move(types),std::move(rows));
    }
}

//transform rawMySQLReturnValue to ResType
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



static void transform_to_rawMySQLReturnValue(rawMySQLReturnValue & str,ResType & item ){
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
                cur+=str.rowValues[i][j]+=",";
            }else{
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

static metadata_files load_meta(string db="tdb", string table="student", string filename="metadata.data"){
    metadata_files mf;
    mf.set_db(db);
    mf.set_table(table);
    mf.deserialize(filename);
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

template<class T>
vector<T> flat_vec(vector<vector<T>> &input){
    vector<T> res;
    for(auto item:input){
        for(auto i:item){
            res.push_back(i);
        }
    }
    return res;
}
struct batch{
    vector<string> field_names;
    vector<int> field_types;
    vector<int> field_lengths;
};

static
batch get_batch(metadata_files & input,std::vector<transField> &tfms){
    vector<vector<int>> selected_field_types = input.get_selected_field_types();
    vector<vector<int>> selected_field_lengths = input.get_selected_field_lengths();
    vector<vector<string>> selected_field_names = input.get_selected_field_names();
    vector<int> dec_onion_index = input.get_dec_onion_index();
    vector<string> has_salt = input.get_has_salt();
   
    vector<string> field_names;
    vector<int> field_types;
    vector<int> field_lengths;

    for(auto i=0u;i<tfms.size();i++){
         int index = dec_onion_index[i];
         string dec_field_name = tfms[i].fields[index];
         auto f =  find(selected_field_names[i].begin(),selected_field_names[i].end(),dec_field_name);
         assert(f!=selected_field_names[i].end());
         int j = f - selected_field_names[i].begin();
         if(has_salt[i]==string("true")){
             field_names.push_back(selected_field_names[i][j]);
             field_types.push_back(selected_field_types[i][j]);
             field_lengths.push_back(selected_field_lengths[i][j]);

             field_names.push_back(selected_field_names[i].back());
             field_types.push_back(selected_field_types[i].back());
             field_lengths.push_back(selected_field_lengths[i].back());             
         }else{
             assert(1==2);
         }
    }
    batch bt;
    bt.field_names = field_names;
    bt.field_types = field_types;
    bt.field_lengths = field_lengths;
    return bt;
}

#include <algorithm>
static vector<vector<string>> load_table_fields(metadata_files & input,std::vector<transField> &tfms) {
    string db = input.get_db();
    string table = input.get_table();
    vector<vector<string>> res;
    string prefix = string("data/")+db+"/"+table+"/";

    auto bt = get_batch(input,tfms);
    vector<string> field_names = bt.field_names;
    vector<int> field_types = bt.field_types;
    vector<int> field_lengths = bt.field_lengths;
 
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
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
    //get all the fields in the tables.
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);
    auto res = getTransField(fms);
    std::vector<enum_field_types> types;//Added
    metadata_files res_meta = load_meta(db,table);
    std::shared_ptr<ReturnMeta> rm = getReturnMeta(fms,res);
    //why do we need this??
    create_embedded_thd(0);
    rawMySQLReturnValue resraw;
    //load fields in the stored file
    vector<vector<string>> res_field = load_table_fields(res_meta,res);
    resraw.rowValues = res_field;

//    auto field_names = flat_vec(res_meta.selected_field_names);
//    auto field_types = flat_vec(res_meta.selected_field_types);
//    auto field_lengths = flat_vec(res_meta.selected_field_lengths);

    auto bt = get_batch(res_meta,res);
    vector<string> field_names = bt.field_names;
    vector<int> field_types = bt.field_types;
    vector<int> field_lengths = bt.field_lengths;

    resraw.fieldNames = field_names;
    for(unsigned int i=0;i<field_types.size();++i){
	resraw.fieldTypes.push_back(static_cast<enum_field_types>(field_types[i]));
    }
    ResType rawtorestype = MygetResTypeFromLuaTable(false, &resraw);
    auto finalresults = decryptResults(rawtorestype,*rm);
    return finalresults;
}

int
main(int argc, char* argv[]){
    init();
    create_embedded_thd(0);
    std::string db="tdb",table="student";
    globalEsp = (char*)malloc(sizeof(char)*5000);
    if(globalEsp==NULL){
        printf("unable to allocate esp\n");
        return 0;
    }
    /*load and decrypt*/
    ResType res =  load_files(db,table);
    /*transform*/
    rawMySQLReturnValue str;
    transform_to_rawMySQLReturnValue(str,res);
    std::vector<string> res_query;
    /*get piped insert*/
    construct_insert(str,table,res_query);
    for(auto item:res_query){
        cout<<item<<endl;
    }
    free(globalEsp);
    /*the next step is to construct encrypted insert query*/
    return 0;
}

