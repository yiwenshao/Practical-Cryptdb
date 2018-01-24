#pragma once

/*
added by et. all et related things.
*/

#include<assert.h>
#include <stdlib.h>
#include <stdio.h>

struct globalConstants{
    int loadCount;/*used to limit the number of final_load*/
};

extern const char* cryptdb_dir;
extern const int gtoken;
extern const globalConstants constGlobalConstants;

globalConstants initGlobalConstants();
