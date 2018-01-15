#pragma once

/*class metadata_file{
    string db,table;
    int num_of_fields;
    vector<string> field_types;
    vector<int> field_lengths;
    vector<string> field_names;
    vector<int> choosen_onions;
public:
    void set_db(std::string idb){db=idb;}
    std::string get_db(){return db;}
    void set_table(std::string itable){table=itable;}
    std::string get_table(){return table;}
    void set_db_table(std::string idb,std::string itable){db=idb;table=itable;}
    void set_num_of_fields(int num){num_of_fields = num;}
    int get_num_of_fields(){return num_of_fields;}
    void set_field_types(vector<string> input){field_types = input;}
    std::vector<std::string> & get_field_types(){return field_types;}
    void set_field_lengths(vector<int> input){field_lengths = input;}
    std::vector<int> & get_field_lengths(){return field_lengths;}
    void set_field_names(vector<string> input){field_names = input;}
    std::vector<std::string> & get_field_names(){return field_names;}
    void set_choosen_onions(vector<int> input){choosen_onions = input;}
    std::vector<int>& get_choosen_onions(){return choosen_onions;}
    void serilize();
    void deserialize(std::string filename);
    void show();
    static bool make_path(std::string directory){
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
};

void metadata_file::serilize(){
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

    s = string("num_of_fields:")+to_string(num_of_fields)+"\n";
    fwrite(s.c_str(),1,s.size(),localmeta);    

    s = string("field_types:");
    for(auto item:field_types){
        s+=item+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);    

    s = string("field_lengths:");
    for(auto item : field_lengths){
        s+=to_string(item)+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);

    s = string("field_names:");
    for(auto item : field_names){
        s+=item+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);

    s = string("choosen_onions:");
    for(auto item : choosen_onions){
        s+=to_string(item)+=" ";
    }
    s.back()='\n';
    fwrite(s.c_str(),1,s.size(),localmeta);
    fclose(localmeta);
}

void metadata_file::deserialize(std::string filename){
    filename = string("data/")+db+"/"+table+"/"+filename;
    std::ifstream infile(filename);
    string line;
    while(std::getline(infile,line)){
        int index = line.find(":");
        string head = line.substr(0,index);
        if(head=="database"){
            set_db(line.substr(index+1));
        }else if(head=="table"){
            set_table(line.substr(index+1));
        }else if(head=="num_of_fields"){
            set_num_of_fields(std::stoi(line.substr(index+1)));
        }else if(head=="field_types"){
            string types = line.substr(index+1);
            int start=0,next=0;
            std::vector<std::string> tmp;
            while((next=types.find(' ',start))!=-1){
                string item = types.substr(start,next-start);
                tmp.push_back(item);
                start = next+1;
            }
            string item = types.substr(start);
            tmp.push_back(item);
            set_field_types(tmp);
        }else if(head=="field_lengths"){
            string lengths = line.substr(index+1);
            int start=0,next=0;
            std::vector<int> tmp;
            while((next=lengths.find(' ',start))!=-1){
                string item = lengths.substr(start,next-start);
                tmp.push_back(std::stoi(item));
                start = next+1;
            }
            string item = lengths.substr(start);
            tmp.push_back(std::stoi(item));
            set_field_lengths(tmp);
        }else if(head=="field_names"){
            std::vector<std::string> tmp;
            string names = line.substr(index+1);
            int start=0,next=0;
            while((next=names.find(' ',start))!=-1){
                string item = names.substr(start,next-start);
                tmp.push_back(item);
                start = next+1;
            }
            string item = names.substr(start);
            tmp.push_back(item);
            set_field_names(tmp);
        }else if(head=="choosen_onions"){
            std::vector<int> tmp;
            string c_onions = line.substr(index+1);
            int start=0,next=0;
            while((next=c_onions.find(' ',start))!=-1){
                string item = c_onions.substr(start,next-start);
                tmp.push_back(std::stoi(item));
                start = next+1;
            }
            string item = c_onions.substr(start);
            tmp.push_back(std::stoi(item));
            set_choosen_onions(tmp);
        }    
    }
    infile.close();
}

void metadata_file::show(){
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
*/



/*******************************************************************************************************************/

class metadata_files{
public:
    string db,table;
    /*selected fields*/
    vector<vector<int>> selected_field_types;
    vector<vector<int>> selected_field_lengths;
    vector<vector<string>> selected_field_names;
    vector<string> has_salt;
    vector<int> dec_onion_index;/*should be 0,1,2,3...*/
    std::string serialize_vec_int(std::string s,vector<int> vec_int){
        s+=":";
        for(auto item:vec_int){
            s+=to_string(item)+=" ";
        }
        s.back()='\n';
        return s;
    }
    std::string serialize_vec_str(std::string s,vector<string> vec_str){
        s+=":";
        for(auto item:vec_str){
            s+=item+=" ";
        }
        s.back()='\n';
        return s;
    }
    vector<string> string_to_vec_str(string line){
        int start=0,next=0;
        std::vector<std::string> tmp;
        while((next=line.find(' ',start))!=-1){
            string item = line.substr(start,next-start);
            tmp.push_back(item);
            start = next+1;
        }
        string item = line.substr(start);
        tmp.push_back(item);
        return tmp;
    }

    vector<int> string_to_vec_int(string line){
        int start=0,next=0;
        std::vector<int> tmp;
        while((next=line.find(' ',start))!=-1){
            string item = line.substr(start,next-start);
            tmp.push_back(std::stoi(item));
            start = next+1;
        }
        string item = line.substr(start);
        tmp.push_back(std::stoi(item));
        return tmp;
    }
public:
    void set_db(std::string idb){db=idb;}
    std::string get_db(){return db;}
    void set_table(std::string itable){table=itable;}
    std::string get_table(){return table;}
    void set_db_table(std::string idb,std::string itable){db=idb;table=itable;}
    void set_selected_field_types(vector<vector<int>> input){selected_field_types = input;}
    void set_selected_field_lengths(vector<vector<int>> input){selected_field_lengths = input;}
    void set_selected_field_names(vector<vector<string>> input){selected_field_names = input;}
    void set_dec_onion_index(vector<int> input){dec_onion_index = input;}
    void set_has_salt(vector<string> input){has_salt = input;}
    void serialize();
    void deserialize(std::string filename);

    static bool make_path(std::string directory){
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
};

void metadata_files::serialize(){
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

void metadata_files::deserialize(std::string filename){
    filename = string("data/")+db+"/"+table+"/"+filename;
    std::ifstream infile(filename);
    string line;
    while(std::getline(infile,line)){
        int index = line.find(":");
        string head = line.substr(0,index);        
        if(head == "INDEX"){
            //INIT HERE
            selected_field_types.push_back(vector<int>());
            selected_field_lengths.push_back(vector<int>());
            selected_field_names.push_back(vector<string>());
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
