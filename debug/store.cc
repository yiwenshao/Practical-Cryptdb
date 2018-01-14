#include "debug/store.hh"

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


//query for testing purposes
static
std::string getTestQuery(SchemaInfo &schema, std::vector<transField> &tfds,
                                     std::string db="tdb",std::string table="student1"){
    std::string res = "SELECT ";
    const std::unique_ptr<IdentityMetaKey> dbmeta_key(new IdentityMetaKey(db));
    //get databaseMeta, search in the map
    DatabaseMeta * dbm = schema.getChild(*dbmeta_key);
    const TableMeta & tbm = *((*dbm).getChild(IdentityMetaKey(table)));
    std::string annotablename = tbm.getAnonTableName();    
    //then a list of onion names
    for(auto item:tfds){
        for(auto index:item.choosenOnions){
            res += item.fields[index];
            res += " , ";
        }
    	if(item.hasSalt){
            res += item.originalFm->getSaltName()+" , ";
        }
    }
    res = res.substr(0,res.size()-2);
    res = res + "FROM `"+db+std::string("`.`")+annotablename+"`";
    return res;
}

/*
    only support relative path
*/
static bool make_path(string directory){
    struct stat st;
    if(directory.size()==0||directory[0]=='/') return false;
    if(directory.back()=='/') directory.pop_back();
    int start = 0,next=0;
    while(stat(directory.c_str(),&st)==-1&&next!=-1){
        next = directory.find('/',start);
        if(next!=-1){
            string sub = directory.substr(0,next);
            if(stat(sub.c_str(),&st)==-1)
                mkdir(sub.c_str(),0700);
            start =  next + 1;
        }else{
            mkdir(directory.c_str(),0700);
        }
    }
    return true;
}


static void write_meta(rawMySQLReturnValue& resraw,string db,string table){
    //write metadata
    FILE * localmeta = NULL;
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
