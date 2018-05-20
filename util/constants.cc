#include "util/constants.hh"
#include "util/util.hh"
#include <string>
#include <fstream>


const char *cryptdb_dir = getenv("CRYPTDB_DIR");

//assert(cryptdb_dir!=NULL);
const globalConstants constGlobalConstants = initGlobalConstants();
globalConstants initGlobalConstants(){
//    printf("%s",cryptdb_dir);
    assert(cryptdb_dir != NULL);
    assert(cryptdb_dir[0]=='/');
    std::string prefix = std::string(cryptdb_dir);
    std::string filename = prefix+"/conf/"+std::string("global.constant");
    
    std::ifstream infile(filename);

    globalConstants res;
    std::string line;
    while(std::getline(infile,line)) {
        if(line.size()==0) continue;
        if(line[0]=='#'||line[0]=='\n') continue;
        int index = line.find(":");
        std::string head = line.substr(0,index);
        if(head=="loadCount") {
            res.loadCount = stoi(line.substr(index+1));          
        }else if(head=="pipelineCount") {
            res.pipelineCount = stoi(line.substr(index+1));
        }else if(head=="useASHE") {
            if(line.substr(index+1)=="true" ){
                res.useASHE = true;
            }else{
                res.useASHE = false;
            }
        }else if(head=="useHOM") {
            if(line.substr(index+1)=="true"){
                res.useHOM = true;
            }else{
                res.useHOM = false;
            }
        }else if(head=="useOPE") {
            if(line.substr(index+1)=="true"){
                res.useOPE = true;
            }else{
                res.useOPE = false;
            }
        }else if(head=="useSWP") {
            if(line.substr(index+1)=="true"){
                res.useSWP = true;
            }else{
                res.useSWP = false;
            }
        }else if(head=="useDET") {
            if(line.substr(index+1)=="true"){
                res.useDET = true;
            }else{
                res.useDET = false;
            }
        }else if(head=="useSalt"){
            if(line.substr(index+1)=="true"){
                res.useSalt = true;
            }else{
                res.useSalt = false;
            }
        }else if(head=="other") {
            ;
        }else if(head == "BS_STR"){
            std::string sub = line.substr(index+1);
            if(sub=="NA"){
                res.BS_STR = 0;
            }else if(sub=="MIN"){
                res.BS_STR = 1;
            }else if(sub=="MIDEAN"){
                res.BS_STR = 2;
            }else if(sub=="FULL"){
                res.BS_STR = 3;
            }else{
                POINT
                assert(0);
            }
        }else if(head == "BS_IA"){
            std::string sub = line.substr(index+1);
            if(sub=="NA"){
                res.BS_IA = 0;
            }else if(sub=="MIN"){
                res.BS_IA = 1;
            }else if(sub=="MIDEAN"){
                res.BS_IA = 2;
            }else if(sub=="FULL"){
                res.BS_IA = 3;
            }else{
                POINT
                assert(0);
            }
        }else if(head == "BS_IH"){
            std::string sub = line.substr(index+1);
            if(sub=="NA"){
                res.BS_IH = 0;
            }else if(sub=="MIN"){
                res.BS_IH = 1;
            }else if(sub=="MIDEAN"){
                res.BS_IH = 2;
            }else if(sub=="FULL"){
                res.BS_IH = 3;
            }else{
                POINT
                assert(0);
            }
        }else if(head == "USE_ASHE") {
            std::string sub = line.substr(index+1);
            if(sub=="true"){
                res.USE_ASHE = true;
            }else if(sub=="false"){
                res.USE_ASHE = false;
            }else{
                POINT
                assert(0);
            }
        }else {
            POINT
            assert(0);
        }
    }
    /* the following values need not be determined 
       at runtime.*/
    res.logFile="LOG.TXT";
    infile.close(); 
    return res;
}

