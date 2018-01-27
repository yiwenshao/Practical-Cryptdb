#include "wrapper/reuse.hh"
#include "util/util.hh"
#include "util/constants.hh"
#include <map>

using std::cout;
using std::cin;
using std::endl;
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

void FieldMeta_Wrapper::show(){
        for(auto i=0U;i<fields.size();i++){
             //cout<<fields[i]<<" : "<<gmp2[onions[i]]<<"\t";
        }
        cout<<endl;
        if(hasSalt){
            cout<<"has salt"<<endl;
        }else cout<<"do not have salt"<<endl;
}


void FieldMetaTrans::trans(FieldMeta *fm) {
    originalFm=fm;
    if(fm->getHasSalt()){
        hasSalt = true;
        saltName = fm->getSaltName();
    }else{
        hasSalt = false;        
    }
    for(std::pair<const OnionMetaKey *, OnionMeta *> &omkv:fm->orderedOnionMetas()){
        onion o = omkv.first->getValue();
        OnionMeta* om = omkv.second;
        onionsO.push_back(o);
//        onionsStr.push_back(TypeText<onion>::toText(o));
        onionsOm.push_back(om);
        onionsName.push_back(om->getAnonOnionName());
    }
}


void FieldMetaTrans::choose(std::vector<onion> onionSet){
    choosenOnionO = onionSet;
    for(auto &o:onionSet){
            choosenOnionName.push_back(originalFm->getOnionMeta(o)->getAnonOnionName()
        );
    }
}

void FieldMetaTrans::choose(std::vector<int> onionIndexSet){
    choosenIndex = onionIndexSet;
    for(auto index:onionIndexSet){
        choosenOnionO.push_back(onionsO[index]);
        choosenOnionName.push_back(onionsName[index]);
    }
}




Item_null *
make_null(const std::string &name){
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}



std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null());
    }
    return out;
}

ResType rawMySQLReturnValue_to_ResType(bool isNULL,rawMySQLReturnValue *inRow,int in_last_insert_id){
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

void
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

void
addSaltToReturn(ReturnMeta *const rm, int pos) {
    const bool test = static_cast<unsigned int>(pos) == rm->rfmeta.size();
    TEST_TextMessageError(test, "ReturnMeta has badly ordered"
                                " ReturnFields!");
    std::pair<int, ReturnField>
        pair(pos, ReturnField(true, "", OLK::invalidOLK(), -1));
    rm->rfmeta.insert(pair);
}

std::vector<FieldMeta *> getFieldMeta(SchemaInfo &schema,std::string db, std::string table){
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


std::unique_ptr<SchemaInfo> myLoadSchemaInfo(std::string embeddedDir) {
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

Item *
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

std::vector<FieldMeta_Wrapper> FieldMeta_to_Wrapper(std::vector<FieldMeta *> pfms){
    std::vector<FieldMeta_Wrapper> res;
    //for every field
    for(auto pfm:pfms){
        FieldMeta_Wrapper tf;
	    tf.originalFm = pfm;
        for(std::pair<const OnionMetaKey *, OnionMeta *> &ompair:
                                               pfm->orderedOnionMetas()){
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

void transform_to_rawMySQLReturnValue(rawMySQLReturnValue & str,ResType & item ){
    for(auto row : item.rows){
        std::vector<std::string> temp;
        for(auto item : row){
            temp.push_back(ItemToString(*item));
        }
        str.rowValues.push_back(temp);
    }
    str.fieldTypes = item.types;
}

rawMySQLReturnValue 
executeAndGetResultRemote(Connect * curConn,std::string query){
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


MySQLColumnData
executeAndGetColumnData(Connect * curConn,std::string query){
    std::unique_ptr<DBResult> dbres;
    curConn->execute(query, &dbres);
    MySQLColumnData myRaw;
    
    if(dbres==nullptr||dbres->n==NULL){
        std::cout<<"no results"<<std::endl;
        return myRaw;
    }

    int num = mysql_num_rows(dbres->n);
    int numOfFields = mysql_num_fields(dbres->n);

    MYSQL_FIELD *field;
    MYSQL_ROW row;

    while( (field = mysql_fetch_field(dbres->n)) ) {
        myRaw.fieldNames.push_back(std::string(field->name));
        myRaw.fieldTypes.push_back(field->type);
        myRaw.maxLengths.push_back(field->max_length);
    }

    for(int i=0;i<numOfFields;i++){
        myRaw.columnData.push_back(std::vector<std::string>());
    }

    if(num!=0){
        while( (row = mysql_fetch_row(dbres->n)) ){
            //what's the difference between fieldlen
	    unsigned long * fieldLen = mysql_fetch_lengths(dbres->n);
            for(int i=0;i<numOfFields;i++){
                if(row[i]==NULL) myRaw.columnData[i].push_back("NULL");
                else myRaw.columnData[i].push_back(std::string(row[i],fieldLen[i]));
            }
        }
    }
    return myRaw;
}


rawMySQLReturnValue 
executeAndGetResultRemoteWithOneVariableLen(Connect * curConn,
                                           std::string query,
                                           std::vector<int> &vlen,
                                           std::vector<std::string> &vstr,
                                           std::string &vname) {
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


void
write_row_data(rawMySQLReturnValue& resraw,std::string db,std::string table,std::string prefix){
    std::vector<FILE*> data_files;
    prefix = prefix+db+"/"+table+"/";
    for(auto item:resraw.fieldNames){
        item=prefix+item;
        FILE * data  = fopen(item.c_str(),"w");
        data_files.push_back(data);
    }
    const std::string token = "\n";
    for(auto &item : resraw.rowValues){        
        for(unsigned int i=0u;i<item.size();i++){
           fwrite(item[i].c_str(),1,item[i].size(),data_files[i]);
           if(IS_NUM(resraw.fieldTypes[i])){
               fwrite(token.c_str(),1,token.size(),data_files[i]);
           }
        }
    }
    for(auto item:data_files){
        fclose(item);
    }
}

/* Write a column of data of the type string in mysql. one line per record. 
   string should be escaped before being written into the file */
void
writeColumndataEscapeString(const std::vector<std::string> &column,
                      std::string columnFilename,
                      unsigned int maxLength) {
    FILE* dataFileHandler = fopen(columnFilename.c_str(),"w");
    char *buf = new char[2*maxLength+1u];
    const std::string token = "\n";
    for(auto &item:column){
        size_t len = escape_string_for_mysql_modify(buf,item.c_str(),item.size());
        fwrite(buf,1,len,dataFileHandler);
        fwrite(token.c_str(),1,token.size(),dataFileHandler);
    }
    fclose(dataFileHandler);
    delete [] buf;
}

/* write a column of data in type integer.
   one record per line 
*/
void 
writeColumndataNum(const std::vector<std::string> &column,
                      std::string columnFilename) {
    FILE* dataFileHandler = fopen(columnFilename.c_str(),"w");
    const std::string token = "\n";
    for(auto &item:column) {
        fwrite(item.c_str(),1,item.size(),dataFileHandler);
        fwrite(token.c_str(),1,token.size(),dataFileHandler);        
    }
    fclose(dataFileHandler);
}


void loadFileEscape(std::string filename,
                    std::vector<std::string> &res,
                    unsigned int maxLength) {
    std::ifstream infile(filename);
    std::string line;
    char *buf = new char[2*maxLength+1u];
    while(std::getline(infile,line)){
        size_t len = reverse_escape_string_for_mysql_modify(buf,line.c_str());
        std::string temp(buf,len);
        res.push_back(temp);
    }
    infile.close();
}

void loadFileEscapeLimitCount(std::string filename,
                    std::vector<std::string> &res,
                    unsigned int maxLength,int limit) {
    std::ifstream infile(filename);
    std::string line;
    char *buf = new char[2*maxLength+1u];
    int localCount=0;
    while(std::getline(infile,line)){
        size_t len = reverse_escape_string_for_mysql_modify(buf,line.c_str());
        std::string temp(buf,len);
        res.push_back(temp);
        localCount++;
        if(localCount==limit) break;
    }
    infile.close();
}


void 
loadFileNoEscape(std::string filename,            
              std::vector<std::string> &res) {
    std::ifstream infile(filename);                  
    std::string line;
    while(std::getline(infile,line)) {
        res.push_back(line);
    }
}

void 
loadFileNoEscapeLimitCount(std::string filename,
                 std::vector<std::string> &res,int limit){
    std::ifstream infile(filename);                  
    std::string line;
    int localCount=0;
    while(std::getline(infile,line)) {
        res.push_back(line);
        localCount++;
        if(localCount==limit) break;
    }
}





STORE_STRATEGY currentStrategy = STORE_STRATEGY::ALL;

/*storage used when we store*/
void storeStrategies(std::vector<FieldMetaTrans>& res){    
    if(currentStrategy == STORE_STRATEGY::FIRST){
        std::vector<int> in{0};
        for(auto &item:res){
            item.choose(in);
        }
    }else if(currentStrategy == STORE_STRATEGY::ALL){
        for(auto &item:res){
            item.chooseAll();
        }
    }else{
        exit(0);
    }
    //Stored onions should be showed here
    for(auto &item:res){
        item.showChoosenOnionO();
    }
}


static const std::vector<onion> onion_order = {
        oDET,
        oOPE,
        oASHE,
        oAGG, 
};

int getDecryptionOnionIndex(FieldMetaTrans& fdtrans) {
    int res = -1;
    auto onionsO = fdtrans.getChoosenOnionO();
    std::map<onion,unsigned int> onionIndexPair;
    for(unsigned int i=0u;i<onionsO.size();i++){
        onionIndexPair[onionsO[i]]=i;
    }
    for(auto item:onion_order){
        if(onionIndexPair.find(item)!=onionIndexPair.end()){
            if( ((item==oDET)&&(constGlobalConstants.useDET==true)) ||
                ((item==oOPE)&&(constGlobalConstants.useOPE==true)) ||
                ((item==oASHE)&&(constGlobalConstants.useASHE==true)) ||
                ((item==oAGG)&&(constGlobalConstants.useHOM==true))
              ){
                res = onionIndexPair[item];
                break;
            }
        }
    }
    assert(res!=-1);
    return res;
}


void load_num_file(std::string filename,std::vector<std::string> &res){
    std::ifstream infile(filename);
    std::string line;
    while(std::getline(infile,line)){
        res.push_back(std::move(line));
    }
    infile.close();
}

void 
load_num_file_count(std::string filename,
              std::vector<std::string> &res,
	      int count) {
    std::ifstream infile(filename);
    std::string line;
    int localCount = 0;
    while(std::getline(infile,line)){
        res.push_back(std::move(line));
        localCount++;
        if(localCount==count)
            break;
    }
    infile.close();
}

void load_string_file(std::string filename, 
                      std::vector<std::string> &res, 
                      unsigned long length) {
    char *buf = new char[length];
    int fd = open(filename.c_str(),O_RDONLY);
    if(fd==-1) assert(0);//reading from -1 may cause errors
    while(read(fd,buf,length)!=0){
        res.push_back(std::move(std::string(buf,length)));
    }
    delete buf;
    close(fd);
}


void
load_string_file_count(std::string filename, 
                       std::vector<std::string> &res,
                       unsigned long length,
                       int count) {
    char *buf = new char[length];
    int localCount=0;
    int fd = open(filename.c_str(),O_RDONLY);
    if(fd==-1) assert(0);//reading from -1 may cause errors
    while(read(fd,buf,length)!=0){
        res.push_back(std::move(std::string(buf,length)));
        localCount++;
        if(localCount==count)
            break;
    }
    delete buf;
    close(fd);
}


std::ostream&
insertManyValues(std::ostream &out,List<List_item> &newList){
    out << " VALUES " << noparen(newList)<<";";
    return out;
}



std::unique_ptr<Item>
getIntItem(int i){
    //Should cast, or we get strange value
    Item * it = new Item_int(i);
    return std::unique_ptr<Item>(it);
}

std::unique_ptr<Item>
getStringItem(std::string s){
    Item * it = new Item_string(make_thd_string(s),s.length(),&my_charset_bin);
    return std::unique_ptr<Item>(it);

}


