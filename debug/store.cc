#include "debug/store.hh"
#include "debug/common.hh"



static void write_meta(rawMySQLReturnValue& resraw,std::vector<transField> &res,string db,string table){
    metadata_files mf;
    mf.set_db_table(db,table);

    vector<vector<int>> selected_field_types;
    vector<vector<int>> selected_field_lengths;
    vector<vector<string>> selected_field_names;
    vector<int> dec_onion_index;
    vector<string> has_salt;
    for(auto item:res){
        vector<int> field_types;
        vector<int> field_lengths;
        vector<string> field_names=item.fields;
        int onion_index = item.onionIndex;
        for(auto tp:resraw.fieldTypes)
            field_types.push_back(static_cast<int>(tp));
        field_lengths = resraw.lengths;
        if(item.hasSalt){
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
static void write_row_data(rawMySQLReturnValue& resraw,string db, string table){
    vector<FILE*> data_files;
    string prefix = string("data/")+db+"/"+table+"/";
    for(auto item:resraw.fieldNames){
        item=prefix+item;
        FILE * data  = fopen(item.c_str(),"w");
        data_files.push_back(data);
    }
    const string token = "\n";
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
static
void write_raw_data_to_files(rawMySQLReturnValue& resraw,std::vector<transField> &res ,string db,string table){
    //write metafiles
    write_meta(resraw,res,db,table);
    //write datafiles
    write_row_data(resraw,db,table);
}
static void store(std::string db, std::string table){
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
    //get all the fields in the tables
    std::vector<FieldMeta*> fms = getFieldMeta(*schema,db,table);
    //transform the field so that selected onions can be used
    std::vector<transField> res = getTransField(fms);
    for(auto &item:res){
        (void)item;
        item.choosenOnions.push_back(0);
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
