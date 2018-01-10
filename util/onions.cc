#include "util/onions.hh"

std::map<std::string,onion> string_to_onion{
    {"oDET",oDET},
    {"oOPE",oOPE},
    {"oAGG",oAGG},
    {"oSWP",oSWP},
    {"oPLAIN",oPLAIN},
    {"oBESTEFFORT",oBESTEFFORT},
    {"oASHE",oASHE},
    {"oINVALID",oINVALID}
};


std::map<std::string,SECLEVEL> string_to_seclevel={
    {"INVALID",SECLEVEL::INVALID},
    {"PLAINVAL",SECLEVEL::PLAINVAL},
    {"OPEFOREIGN",SECLEVEL::OPEFOREIGN},
    {"OPE",SECLEVEL::OPE},
    {"DETJOIN",SECLEVEL::DETJOIN},
    {"DET",SECLEVEL::DET},
    {"SEARCH",SECLEVEL::SEARCH},
    {"HOM",SECLEVEL::HOM},
    {"ASHE",SECLEVEL::ASHE},
    {"RND",SECLEVEL::RND}
};
