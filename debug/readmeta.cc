#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;
#include "debug/common.hh"

int main(){
    metadata_files mf;
    mf.set_db_table("tdb","student");
    mf.deserialize("metadata.data");
    return 0;
}
