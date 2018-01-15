#pragma once
class metadata_file{
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


