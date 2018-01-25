#include "util/log.hh"
#include <assert.h>
#include <stdio.h>

logToFile::logToFile(std::string filename){
    logfileHandler = fopen(filename.c_str(),"w");
    assert(logfileHandler!=NULL);
}

logToFile&
logToFile::operator<<(std::string record) {
    fwrite(record.c_str(),1,record.size(),logfileHandler);
    return *this;
}

logToFile::~logToFile(){
    fclose(logfileHandler);
}
