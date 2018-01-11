#include "util/onions.hh"
const char* const dir = 
               (char*)"/home/casualet/github/Practical-Cryptdb/conf/CURRENT.conf";


//static 
onionlayout PLAIN_ONION_LAYOUT = {
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})}
};

/***************************ofthen used*******************************************************/
onionlayout NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};

onionlayout STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN, SECLEVEL::OPE, SECLEVEL::RND})},
    {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                  // SECLEVEL::RND})}
};

/*******************************************************************************************
***************************** Onion layout for numeric data ********************************
********************************************************************************************
*/



//static 
onionlayout NUM_ONION_LAYOUT_NOFOREIGN = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};

//static 
onionlayout NUM_ONION_LAYOUT_NORND = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};


//static 
onionlayout NUM_ONION_LAYOUT_ONLYASHE = {
    {oASHE, std::vector<SECLEVEL>({SECLEVEL::ASHE})}
};

//static 
onionlayout NUM_ONION_LAYOUT_REPLACE_HOM{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET, SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oASHE, std::vector<SECLEVEL>({SECLEVEL::ASHE})}
};

//static 
onionlayout NUM_ONION_LAYOUT_MANYDET = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})}
};

//static 
onionlayout BEST_EFFORT_NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})},
    // Requires SECLEVEL::DET, otherwise you will have to implement
    // encoding for negative numbers in SECLEVEL::RND.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                    SECLEVEL::RND})}
};

//static 
onionlayout NUM_ONION_LAYOUT_TEST{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    {oASHE, std::vector<SECLEVEL>({SECLEVEL::ASHE})}
};

/********************************************************************************************
**************************** Onion layout for str data **************************************
*********************************************************************************************
*/


//static 
onionlayout STR_ONION_LAYOUT_WITHSEARCH{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
};

//static 
onionlayout STR_ONION_LAYOUT_NORND{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
};

//static 
onionlayout BEST_EFFORT_STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
    //                              SECLEVEL::RND})},
    // HACK: RND_str expects the data to be a multiple of 16, so we use
    // DET (it supports decryption UDF) to handle the padding for us.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                    SECLEVEL::RND})}
};








std::map<std::string,onion> string_to_onion{
    {"oDET",oDET},
    {"oOPE",oOPE},
    {"oAGG",oAGG},
    {"oSWP",oSWP},
    {"oPLAIN",oPLAIN},
    {"oBESTEFFORT",oBESTEFFORT},
    {"oASHE",oASHE},
    {"oINVALID",oINVALID}
};


std::map<std::string,SECLEVEL> string_to_seclevel={
    {"INVALID",SECLEVEL::INVALID},
    {"PLAINVAL",SECLEVEL::PLAINVAL},
    {"OPEFOREIGN",SECLEVEL::OPEFOREIGN},
    {"OPE",SECLEVEL::OPE},
    {"DETJOIN",SECLEVEL::DETJOIN},
    {"DET",SECLEVEL::DET},
    {"SEARCH",SECLEVEL::SEARCH},
    {"HOM",SECLEVEL::HOM},
    {"ASHE",SECLEVEL::ASHE},
    {"RND",SECLEVEL::RND}
};

std::vector<std::string> onion_conf::parseline(std::string temp){
    int start = 0;
    int next = 0;
    std::vector<std::string> res;
    while(1){
        char b = temp.back();
        if(b==' '||b=='\n') temp.pop_back();
        else break;
    }
    while((next = temp.find(' ',start))!=-1){
        res.push_back(temp.substr(start,next-start));
        start=next+1;
    }
    res.push_back(temp.substr(start,next-start));
    return res;
}


void onion_conf::read_onionlayout_num(std::string temp){
    std::vector<std::string> res = parseline(temp);
    unsigned int i=1;
    assert(res.size()>1);
    res[0].pop_back();
    std::string onion_name = res[0];
    onions_for_num[onion_name] = std::vector<std::string>();
    for(;i<res.size();i++){
        onions_for_num[onion_name].push_back(res[i]);
    }
}


void onion_conf::read_onionlayout_str(std::string temp){
    std::vector<std::string> res = parseline(temp);
    unsigned int i=1;
    assert(res.size()>1);
    res[0].pop_back();
    std::string onion_name = res[0];
    onions_for_str[onion_name] = std::vector<std::string>();
    for(;i<res.size();i++){
        onions_for_str[onion_name].push_back(res[i]);
    }    

}

onion_conf::onion_conf(const char* f=(char*)"onionlayout.conf"){
    file = fopen(f,"r");
    if(file==NULL) {
        printf("error\n");
    }
    buf = (char*)malloc(sizeof(char)*100);
    std::string current_onion="null";
    size_t n;
    while(getline(&buf,&n,file)!=-1){
        std::string temp(buf);
        if(temp.size()==0||temp[0]=='#'||temp[0]=='\n'){
            continue;
        }
        if(temp.back()=='\n') temp.pop_back();
        if(temp==std::string("[onions for num]")){
            current_onion = "num";
            continue;
        }else if(temp==std::string("[onions for str]")){
            current_onion = "str";
            continue;
        }else if(temp==std::string("[end]")){
            current_onion = "null";
            continue;
        }
        assert(temp.back()!=' ');
        if(current_onion==std::string("num")){
            read_onionlayout_num(temp);
        }else if(current_onion==std::string("str")){
            read_onionlayout_str(temp);
        }else{
            std::cout<<"error status:"<<current_onion<<std::endl;
            return;
        }
    }
    from_string_to_onionlayout();
    fclose(file);
    free(buf);
}

void onion_conf::from_string_to_onionlayout(){
    for(auto onion_levels:onions_for_num){
        std::string onion_name = onion_levels.first;
        assert(string_to_onion.find(onion_name)!=string_to_onion.end());
        onion o = string_to_onion[onion_name];
        onionlayout_for_num[o] = std::vector<SECLEVEL>();
        for(auto item:onion_levels.second){
            assert(string_to_seclevel.find(item)!=string_to_seclevel.end());
            SECLEVEL l = string_to_seclevel[item];
            onionlayout_for_num[o].push_back(l);
        }
    }
    for(auto onion_levels:onions_for_str){
        std::string onion_name = onion_levels.first;
        assert(string_to_onion.find(onion_name)!=string_to_onion.end());
        onion o = string_to_onion[onion_name];
        onionlayout_for_str[o] = std::vector<SECLEVEL>();
        for(auto item:onion_levels.second){
            assert(string_to_seclevel.find(item)!=string_to_seclevel.end());
            SECLEVEL l = string_to_seclevel[item];
            onionlayout_for_str[o].push_back(l);
        }       

    }
}

onion_conf::~onion_conf(){

}


onion_conf global_onion_conf(dir);
onionlayout CURRENT_NUM_LAYOUT = global_onion_conf.get_onionlayout_for_num();
onionlayout CURRENT_STR_LAYOUT = global_onion_conf.get_onionlayout_for_str();


void is_onionlayout_equal(onionlayout &ol1,onionlayout &ol2){
    for(auto item:ol1){
        auto key = item.first;
        assert(ol2.find(key)!=ol2.end());
        assert(ol2[key]==ol1[key]);
    }
}


