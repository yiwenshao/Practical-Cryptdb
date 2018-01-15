#include "debug/store.hh"
#include "debug/common.hh"

static void write_meta(rawMySQLReturnValue& resraw,string db,string table){
    //write metadata
//    FILE * localmeta = NULL;
    metadata_file mf;
    mf.set_db_table(db,table);
    mf.set_num_of_fields(resraw.fieldNames.size());
    std::vector<std::string> temp;
    for(auto item:resraw.fieldTypes){
        temp.push_back(std::to_string(static_cast<int>(item)));
    }
    mf.set_field_types(temp);
    mf.set_field_lengths(resraw.lengths);
    mf.set_field_names(resraw.fieldNames);
    mf.set_choosen_onions(resraw.choosen_onions);
    mf.serilize();
/*
    string prefix = string("data/")+db+"/"+table;
    make_path(prefix);    
    localmeta = fopen((prefix+"/metadata.data").c_str(),"w");
    string s = string("database:")+db;
    s+="\n";
    fwrite(s.c_str(),1,s.size(),localmeta);
    s = string("table:")+table;
    s+="\n";
    fwrite(s.c_str(),1,s.size(),localmeta);
    s = string("num_of_fields:")+to_string(resraw.fieldNames.size())+"\n";
    fwrite(s.c_str(),1,s.size(),localmeta);

    s = string("field_types:");
    for(auto item:resraw.fieldTypes){
        s+=std::to_string(item)+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);

    s = string("field_lengths:");
    for(auto item : resraw.lengths){
        s+=to_string(item)+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);
    s = string("field_names:");
    for(auto item : resraw.fieldNames){
        s+=item+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);
    s = string("choosen_onions:");
    for(auto item : resraw.choosen_onions){
        s+=to_string(item)+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);
    fclose(localmeta);
    */
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
void write_raw_data_to_files(rawMySQLReturnValue& resraw,string db,string table){
    //write metafiles
    write_meta(resraw,db,table);
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
        item.choosenOnions.push_back(0);
    }
    //generate the backup query and then fetch the tuples
    std::shared_ptr<ReturnMeta> rm = getReturnMeta(fms,res);
    std::string backq = getTestQuery(*schema,res,db,table);
    rawMySQLReturnValue resraw =  executeAndGetResultRemote(globalConn,backq);

    for(auto &item:res){
        resraw.choosen_onions.push_back(item.choosenOnions[0]);
    }
    //write the tuples into files
    write_raw_data_to_files(resraw,db,table);
}

int
main(int argc, char* argv[]) {
    init();
    std::string db="tdb",table="student";
    store(db,table);
    return 0;
}
