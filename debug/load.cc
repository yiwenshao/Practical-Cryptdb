#include "debug/load.hh"
#include "debug/common.hh"
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

static vector<vector<string>> load_table_fields(metadata_files & input) {
    string db = input.get_db();
    string table = input.get_table();
    vector<vector<string>> res;
    string prefix = string("data/")+db+"/"+table+"/";

    vector<string> datafiles;
    auto field_names = flat_vec(input.selected_field_names);
    auto field_types = flat_vec(input.selected_field_types);
    auto field_lengths = flat_vec(input.selected_field_lengths);

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
    for(auto item:fms){
        types.push_back(item->getSqlType());
    }//Add new field form FieldMeta
    if(types.size()==1){
        //to be
    }
    metadata_files res_meta = load_meta(db,table);
    std::shared_ptr<ReturnMeta> rm = getReturnMeta(fms,res);

    //why do we need this??
    create_embedded_thd(0);

    rawMySQLReturnValue resraw;

    //load fields in the stored file
    vector<vector<string>> res_field = load_table_fields(res_meta);
    resraw.rowValues = res_field;
    auto field_names = flat_vec(res_meta.selected_field_names);
    auto field_types = flat_vec(res_meta.selected_field_types);
    auto field_lengths = flat_vec(res_meta.selected_field_lengths);
    
    resraw.fieldNames = field_names;
    for(unsigned int i=0;i<field_types.size();++i) {
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

