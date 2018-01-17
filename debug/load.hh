#pragma once

/*1. store data as column files, and restore data as plaintext insert query
* 2. plaintext insert query should be able to recover directly
* 3. should be able to used exsisting data to reduce the computation overhead(to be implemented)
*/
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "wrapper/reuse.hh"

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;

static std::string embeddedDir="/t/cryt/shadow";

char * globalEsp=NULL;

int num_of_pipe = 4;

//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;

//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;
