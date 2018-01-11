#pragma once

#include <string>
#include <map>
#include <vector>
#include <stdio.h>
#include <list>
#include <iostream>
#include <vector>
#include <assert.h>

typedef enum onion {
    oDET,
    oOPE,
    oAGG,
    oSWP,
    oPLAIN,
    oBESTEFFORT,
    oASHE,
    oINVALID,
} onion;

extern std::map<std::string,onion> string_to_onion;

//Sec levels ordered such that
// if a is less secure than b.
// a appears before b
// (note, this is not "iff")

enum class SECLEVEL {
    INVALID,
    PLAINVAL,
    OPEFOREIGN,
    OPE,
    DETJOIN,
    DET,
    SEARCH,
    HOM,
    ASHE,//added
    RND,
};

extern std::map<std::string,SECLEVEL> string_to_seclevel;


//Onion layouts - initial structure of onions
typedef std::map<onion, std::vector<SECLEVEL> > onionlayout;

//a set of removed onionlayouts

extern onionlayout STR_ONION_LAYOUT;
extern onionlayout NUM_ONION_LAYOUT;
extern onionlayout PLAIN_ONION_LAYOUT;
extern onionlayout BEST_EFFORT_NUM_ONION_LAYOUT;
extern onionlayout BEST_EFFORT_STR_ONION_LAYOUT;


//commented since it is not used
//typedef std::map<onion, SECLEVEL>  OnionLevelMap;

enum class SECURITY_RATING {PLAIN, BEST_EFFORT, SENSITIVE};


/*
******************************onion_conf**********************************
*/

class onion_conf{
    std::string dir;
    FILE *file;
    char *buf;
    std::map<std::string,std::vector<std::string>> onions_for_num;
    std::map<std::string,std::vector<std::string>> onions_for_str;
    onionlayout onionlayout_for_num;
    onionlayout onionlayout_for_str;

    std::vector<std::string> parseline(std::string temp);
    void read_onionlayout_num(std::string temp);
    void read_onionlayout_str(std::string temp);
    void from_string_to_onionlayout();
public:
    std::map<std::string,std::vector<std::string>>& get_onion_levels_num(){return onions_for_num;}
    std::map<std::string,std::vector<std::string>>& get_onion_levels_str(){return onions_for_str;}
    onionlayout get_onionlayout_for_num(){return onionlayout_for_num;}
    onionlayout get_onionlayout_for_str(){return onionlayout_for_str;}
    
    onion_conf(const char* filename);
    ~onion_conf();
};


extern onionlayout CURRENT_NUM_LAYOUT;
extern onionlayout CURRENT_STR_LAYOUT;


void is_onionlayout_equal(onionlayout &ol1,onionlayout &ol2);


