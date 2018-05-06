#pragma once

#include <string>

class logToFile{
    FILE* logfileHandler;
public:
    logToFile(std::string filename);
    logToFile(){}
    logToFile& operator<<(std::string record);
    ~logToFile();
};
