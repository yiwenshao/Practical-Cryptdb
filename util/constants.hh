#pragma once

/*
added by et. all et related things.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>

struct globalConstants{
    int loadCount;/*used to limit the number of final_load*/
    int pipelineCount;
    bool useASHE;
    bool useHOM;
    bool useOPE;
    bool useSWP;
    bool useDET;
    bool useSalt;
    int BS_STR;
    int BS_IA;
    int BS_IH;
    std::string logFile;/*file name for log*/
};

extern const char* cryptdb_dir;
extern const int gtoken;
extern const globalConstants constGlobalConstants;

globalConstants initGlobalConstants();
