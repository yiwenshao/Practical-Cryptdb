#include "wrapper/common.hh"
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>

using std::ifstream;

//used to init global and embedded db
std::map<std::string, WrapperState*> gclients;
std::string gembeddedDir;

Connect  * globalInit(std::string ip,int port) {
    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    gclients[client] = new WrapperState();    
    //Connect phase
    ConnectionInfo ci(ip, "root", "letmein",port);
    const std::string master_key = "113341234";
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){  
        perror("getcwd error");  
    }
    gembeddedDir = std::string(buffer)+"/shadow";
    SharedProxyState *shared_ps = 
			new SharedProxyState(ci, gembeddedDir , master_key, 
                                            determineSecurityRating());
    assert(0 == mysql_thread_init());
    //we init embedded database here.
    gclients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    gclients[client]->ps->safeCreateEmbeddedTHD();
    //Connect end!!
    return new Connect(ip, ci.user, ci.passwd, port);
}




string metadata_files::serialize_vec_int(string s,vector<int> vec_int){
    s+=":";
    for(auto item:vec_int){
        s+=to_string(item)+=" ";
    }
    s.back()='\n';
    return s;
}
string metadata_files::serialize_vec_str(string s,vector<string> vec_str){
    s+=":";
    for(auto item:vec_str){
        s+=item+=" ";
    }
    s.back()='\n';
    return s;
}


vector<string> metadata_files::string_to_vec_str(string line){
    int start=0,next=0;
    vector<string> tmp;
    while((next=line.find(' ',start))!=-1){
        string item = line.substr(start,next-start);
        tmp.push_back(item);
        start = next+1;
    }
    string item = line.substr(start);
    tmp.push_back(item);
    return tmp;
}


vector<int> metadata_files::string_to_vec_int(string line){
    int start=0,next=0;
    vector<int> tmp;
    while((next=line.find(' ',start))!=-1){
        string item = line.substr(start,next-start);
        tmp.push_back(stoi(item));
        start = next+1;
    }
    string item = line.substr(start);
    tmp.push_back(stoi(item));
    return tmp;
}


bool metadata_files::make_path(string directory){
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


void metadata_files::serialize(std::string prefix){
    FILE * localmeta = NULL;
    prefix = prefix+db+"/"+table;
    make_path(prefix);
    localmeta = fopen((prefix+"/metadata.data").c_str(),"w");

    string s = string("database:")+db;
    s+="\n";
    fwrite(s.c_str(),1,s.size(),localmeta);
    s = string("table:")+table;
    s+="\n";
    fwrite(s.c_str(),1,s.size(),localmeta);
    for(unsigned int i=0;i<dec_onion_index.size();i++){
        s=string("INDEX:")+to_string(i)+"\n";
        fwrite(s.c_str(),1,s.size(),localmeta);
        //then for each index, that is, for each plain field
        auto field_types = selected_field_types[i];
        s = serialize_vec_int("field_types",field_types);
        fwrite(s.c_str(),1,s.size(),localmeta);

        auto field_lengths = selected_field_lengths[i];        
        s = serialize_vec_int("field_lengths",field_lengths);
        fwrite(s.c_str(),1,s.size(),localmeta);

        auto field_names = selected_field_names[i];
        s = serialize_vec_str("field_names",field_names);
        fwrite(s.c_str(),1,s.size(),localmeta);

        auto get_has_salt = has_salt[i];
        s = string("has_salt:")+get_has_salt+string("\n");
        fwrite(s.c_str(),1,s.size(),localmeta);
      
        auto onion_index = to_string(dec_onion_index[i]);
        s = string("onion_index:")+onion_index+string("\n");
        fwrite(s.c_str(),1,s.size(),localmeta);
    }
    fclose(localmeta);
}

void metadata_files::deserialize(std::string filename, std::string prefix){
    filename = prefix+db+"/"+table+"/"+filename;
    ifstream infile(filename);
    string line;
    while(getline(infile,line)){
        int index = line.find(":");
        string head = line.substr(0,index);        
        if(head == "INDEX"){
        }else if(head=="database"){
            set_db(line.substr(index+1));
        }else if(head=="table"){
            set_table(line.substr(index+1));
        }else if(head=="field_types"){
            string types = line.substr(index+1);
            auto res = string_to_vec_int(types);
            selected_field_types.push_back(res);
        }else if(head=="field_lengths"){
            string lengths = line.substr(index+1);
            auto res = string_to_vec_int(lengths);
            selected_field_lengths.push_back(res);
        }else if(head=="field_names"){
            string names = line.substr(index+1);
            auto res = string_to_vec_str(names);
            selected_field_names.push_back(res);
        }else if(head=="has_salt"){
            has_salt.push_back(line.substr(index+1));
        }else if(head=="onion_index"){
            dec_onion_index.push_back(stoi(line.substr(index+1)));
        }else{
            exit(-1);
        }
    }
    infile.close();
}


//-==========================================================


string TableMetaTrans::serialize_vec_int(string s,vector<int> vec_int){
    s+=":";
    for(auto item:vec_int){
        s+=to_string(item)+=" ";
    }
    s.back()='\n';
    return s;
}
string TableMetaTrans::serialize_vec_str(string s,vector<string> vec_str){
    s+=":";
    for(auto item:vec_str){
        s+=item+=" ";
    }
    s.back()='\n';
    return s;
}


vector<string> TableMetaTrans::string_to_vec_str(string line){
    int start=0,next=0;
    vector<string> tmp;
    while((next=line.find(' ',start))!=-1){
        string item = line.substr(start,next-start);
        tmp.push_back(item);
        start = next+1;
    }
    string item = line.substr(start);
    tmp.push_back(item);
    return tmp;
}


vector<int> TableMetaTrans::string_to_vec_int(string line){
    int start=0,next=0;
    vector<int> tmp;
    while((next=line.find(' ',start))!=-1){
        string item = line.substr(start,next-start);
        tmp.push_back(stoi(item));
        start = next+1;
    }
    string item = line.substr(start);
    tmp.push_back(stoi(item));
    return tmp;
}


bool TableMetaTrans::make_path(string directory){
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


void TableMetaTrans::serialize(std::string filename,std::string prefix){
    FILE * localmeta = NULL;
    prefix = prefix+db+"/"+table+"/";
    make_path(prefix);
    localmeta = fopen((prefix+filename).c_str(),"w");

    string s = string("database:")+db;
    s+="\n";
    fwrite(s.c_str(),1,s.size(),localmeta);
    s = string("table:")+table;
    s+="\n";
    fwrite(s.c_str(),1,s.size(),localmeta);

    for(unsigned int i=0u;i<fts.size();i++){
        s=string("INDEX:")+fts[i].getFieldPlainName()+"\n";
        fwrite(s.c_str(),1,s.size(),localmeta);
        //then for each index, that is, for each plain field
        
        s = serialize_vec_str("ChoosenOnionName",fts[i].getChoosenOnionName());
        fwrite(s.c_str(),1,s.size(),localmeta);

        std::vector<int> tmp;
        for(auto item:fts[i].getChoosenOnionO()){
            tmp.push_back(static_cast<int>(item));
        }
        s = serialize_vec_int("choosenOnionO",tmp);
        fwrite(s.c_str(),1,s.size(),localmeta);

        if(fts[i].getHasSalt()){
            s = std::string("hasSalt:true\n");
            fwrite(s.c_str(),1,s.size(),localmeta);
            s = std::string("saltName:")+fts[i].getSaltName()+"\n";
            fwrite(s.c_str(),1,s.size(),localmeta);
            s = std::string("saltType:")+std::to_string(fts[i].getSaltType())+"\n";
            fwrite(s.c_str(),1,s.size(),localmeta);
            s = std::string("saltLength:")+std::to_string(fts[i].getSaltLength())+"\n";
            fwrite(s.c_str(),1,s.size(),localmeta);
        }else{
            s = std::string("hasSalt:false\n");
            fwrite(s.c_str(),1,s.size(),localmeta);            
        }

        s = serialize_vec_int("choosenFieldTypes",fts[i].getChoosenFieldTypes());
        fwrite(s.c_str(),1,s.size(),localmeta);

        s = serialize_vec_int("choosenFieldLengths",fts[i].getChoosenFieldLengths());
        fwrite(s.c_str(),1,s.size(),localmeta);
    }
    fclose(localmeta);
}

void TableMetaTrans::deserialize(std::string filename, std::string prefix){
    filename = prefix+db+"/"+table+"/"+filename;
    ifstream infile(filename);
    string line;
    while(getline(infile,line)){
        int index = line.find(":");
        string head = line.substr(0,index);        
        if(head == "INDEX"){
            FieldMetaTrans ft;
            fts.push_back(ft);
        }else if(head=="database"){
            set_db(line.substr(index+1));
        }else if(head=="table"){
            set_table(line.substr(index+1));
        }else if(head=="choosenOnionO"){
            string onionO=line.substr(index+1);
            auto res = string_to_vec_int(onionO);
            std::vector<onion> tmp;
            for(auto item:res){
                tmp.push_back(static_cast<onion>(item));
            }
            fts.back().setChoosenOnionO(tmp);
        }else if(head=="ChoosenOnionName"){
            string names = line.substr(index+1);
            auto res = string_to_vec_str(names);
            fts.back().setChoosenOnionName(res);
        }else if(head=="choosenFieldTypes"){
            string fieldTypes = line.substr(index+1);
            auto res = string_to_vec_int(fieldTypes);
            fts.back().setChoosenFieldTypes(res);
        }else if(head=="choosenFieldLengths"){
            string fieldLengths = line.substr(index+1);
            auto res = string_to_vec_int(fieldLengths);
            fts.back().setChoosenFieldLengths(res);
        }else if(head=="hasSalt"){
            std::string hasSaltStr = line.substr(index+1);
            if(hasSaltStr=="true"){
                fts.back().setHasSalt(true);
            }else{
                fts.back().setHasSalt(false);
            }
        }else if(head=="saltName"){
            std::string saltName = line.substr(index+1);
            fts.back().setSaltName(saltName);
        }else if(head=="saltType"){
            std::string saltTypeStr = line.substr(index+1);
            fts.back().setSaltType(std::stoi(saltTypeStr));
        }else if(head=="saltLength"){
            std::string saltLengthStr = line.substr(index+1);
            fts.back().setSaltLength(std::stoi(saltLengthStr));
        }else{
            exit(-1);
        }
    }
    infile.close();
}





TableMetaTrans loadTableMetaTrans(string db, string table, string filename){
    TableMetaTrans mf;
    mf.set_db_table(db,table);
    mf.deserialize();
    return mf;
}

std::vector<std::vector<std::string>>
loadTableFieldsForDecryption(std::string db, std::string table,
std::vector<std::string> field_names,std::vector<int> field_types,
std::vector<int> field_lengths){
    std::string prefix = std::string("data/")+db+"/"+table+"/";
    std::vector<std::vector<std::string>> res;
    std::vector<std::string> datafiles;
    for(auto item:field_names){
        datafiles.push_back(prefix+item);
    }
    for(unsigned int i=0u;i<field_names.size();i++){
       std::vector<std::string> column;
       if(IS_NUM(field_types[i])){
           load_num_file(datafiles[i],column);
       }else{
           load_string_file(datafiles[i],column,field_lengths[i]);
       }
       for(unsigned int j=0u; j<column.size(); j++){
           if(j>=res.size()){
               res.push_back(std::vector<std::string>());
           }
           res[j].push_back(column[j]);
       }
    }
    return std::move(res);
}
