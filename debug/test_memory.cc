#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "wrapper/reuse.hh"
#include "wrapper/common.hh"
#include "wrapper/insert_lib.hh"
using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;
static std::string embeddedDir="/t/cryt/shadow";

std::map<std::string,std::vector<std::string>> gfm;

/*init global full backup. */
static
void initGfb(vector<string> field_names,
             vector<int> field_types,
             vector<int>field_lengths,
                                     std::string db,std::string table){
    std::string prefix = std::string("data/")+db+"/"+table+"/";
    for(unsigned int i=0u; i<field_names.size(); i++) {
        std::string filename = prefix + field_names[i];
        std::vector<std::string> column;
        if(IS_NUM(field_types[i])){
            load_num_file(filename,column);
        }else{
            load_string_file(filename,column,field_lengths[i]);
        }
        gfm[field_names[i]] = std::move(column);
    }//get memory 31%
    for(unsigned int i=0u; i<field_names.size(); i++) {
        gfm.erase(field_names[i]);
    }
    return;
}



int
main(int argc, char* argv[]){
    vector<int> lengths{20,15,256,11};
    vector<int> types{8,8,253,8};
    std::vector<std::string> names{"BLWQNYLYOWoEq",
                                   "HZYMDVEERKoOrder",
                                   "HEPOWBSYEVoADD",
                                   "OHGNYTNLSCoASHE",
                                   "cdb_saltNDFPJCCDAO"
                                  };
    initGfb(names, types, lengths, "micro_db", "int_table");
    return 0;
}

