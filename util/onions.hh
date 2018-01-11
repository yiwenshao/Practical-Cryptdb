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

static onionlayout PLAIN_ONION_LAYOUT = {
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL})}
};


/*******************************************************************************************
***************************** Onion layout for numeric data ********************************
********************************************************************************************
*/

static onionlayout NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};

static onionlayout NUM_ONION_LAYOUT_NOFOREIGN = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};

static onionlayout NUM_ONION_LAYOUT_NORND = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})}
};


static onionlayout NUM_ONION_LAYOUT_ONLYASHE = {
    {oASHE, std::vector<SECLEVEL>({SECLEVEL::ASHE})}
};

static onionlayout NUM_ONION_LAYOUT_REPLACE_HOM{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET, SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oASHE, std::vector<SECLEVEL>({SECLEVEL::ASHE})}
};

static onionlayout NUM_ONION_LAYOUT_MANYDET = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})}
};

static onionlayout BEST_EFFORT_NUM_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    {oAGG, std::vector<SECLEVEL>({SECLEVEL::HOM})},
    // Requires SECLEVEL::DET, otherwise you will have to implement
    // encoding for negative numbers in SECLEVEL::RND.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                    SECLEVEL::RND})}
};

static onionlayout NUM_ONION_LAYOUT_TEST{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    {oASHE, std::vector<SECLEVEL>({SECLEVEL::ASHE})}
};

/********************************************************************************************
**************************** Onion layout for str data **************************************
*********************************************************************************************
*/

static onionlayout STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN, SECLEVEL::OPE, SECLEVEL::RND})},
    //{oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                  // SECLEVEL::RND})}
};

static onionlayout STR_ONION_LAYOUT_WITHSEARCH{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE, SECLEVEL::RND})},
    {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
};

static onionlayout STR_ONION_LAYOUT_NORND{
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPE})},
    {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})}
};

static onionlayout BEST_EFFORT_STR_ONION_LAYOUT = {
    {oDET, std::vector<SECLEVEL>({SECLEVEL::DETJOIN, SECLEVEL::DET,
                                  SECLEVEL::RND})},
    {oOPE, std::vector<SECLEVEL>({SECLEVEL::OPEFOREIGN,SECLEVEL::OPE, SECLEVEL::RND})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::SEARCH})},
    // {oSWP, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
    //                              SECLEVEL::RND})},
    // HACK: RND_str expects the data to be a multiple of 16, so we use
    // DET (it supports decryption UDF) to handle the padding for us.
    {oPLAIN, std::vector<SECLEVEL>({SECLEVEL::PLAINVAL, SECLEVEL::DET,
                                    SECLEVEL::RND})}
};


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
