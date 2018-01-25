#include "util/constants.hh"
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
    while(std::getline(infile,line)){
        int index = line.find(":");
        std::string head = line.substr(0,index);
        if(head=="loadCount"){
            res.loadCount = stoi(line.substr(index+1));          
        }else if(head=="pipelineCount"){
            res.pipelineCount = stoi(line.substr(index+1));
        }else if(head=="other"){
            ;
        }else{
            assert(0);
        }
    }

    /* the following values need not be determined 
       at runtime.*/
    res.logFile="LOG.TXT";
    infile.close(); 
    return res;
}

