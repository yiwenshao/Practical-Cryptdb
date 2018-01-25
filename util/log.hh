#pragma once

#include <string>

class logToFile{
    FILE* logfileHandler;
public:
    logToFile(std::string filename);
    logToFile& operator<<(std::string record);
    ~logToFile();
};
