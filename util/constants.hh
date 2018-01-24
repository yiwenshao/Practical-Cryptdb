#pragma once

/*
added by et. all et related things.
*/

#include<assert.h>
#include <stdlib.h>
#include <stdio.h>

extern const char* cryptdb_dir;
extern const int gtoken;

struct globalConstants{
    int loadCount;/*used to limit the number of final_load*/
};

extern const globalConstants constGlobalConstants;

globalConstants initGlobalConstants();
