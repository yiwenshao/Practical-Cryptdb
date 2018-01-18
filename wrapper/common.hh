#pragma once
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



class metadata_files{
public:
    string db,table;
    /*selected fields*/
    vector<vector<int>> selected_field_types;
    vector<vector<int>> selected_field_lengths;
    vector<vector<string>> selected_field_names;
    vector<string> has_salt;
    vector<int> dec_onion_index;/*should be 0,1,2,3...*/

    string serialize_vec_int(string s,vector<int> vec_int);
    string serialize_vec_str(string s,vector<string> vec_str);
    vector<string> string_to_vec_str(string line);
    vector<int> string_to_vec_int(string line);
    static bool make_path(string directory);
public:
    void set_db(string idb){db=idb;}
    string get_db(){return db;}

    void set_table(string itable){table=itable;}
    string get_table(){return table;}

    void set_db_table(string idb,string itable){db=idb;table=itable;}

    void set_selected_field_types(vector<vector<int>> input){selected_field_types = input;}
    vector<vector<int>> &get_selected_field_types(){return selected_field_types;};

    void set_selected_field_lengths(vector<vector<int>> input){selected_field_lengths = input;}
    vector<vector<int>> &get_selected_field_lengths(){return selected_field_lengths;}

    void set_selected_field_names(vector<vector<string>> input){selected_field_names = input;}
    vector<vector<string>> &get_selected_field_names(){return selected_field_names;}

    void set_dec_onion_index(vector<int> input){dec_onion_index = input;}
    vector<int> &get_dec_onion_index(){return dec_onion_index;}

    void set_has_salt(vector<string> input){has_salt = input;}
    vector<string> &get_has_salt(){return has_salt;}

    void serialize(std::string prefix="data/");
    void deserialize(std::string filename, std::string prefix="data/");
};


class TableMetaTrans{
    string db,table;
    std::vector<FieldMetaTrans> fts;

    string serialize_vec_int(string s,vector<int> vec_int);
    string serialize_vec_str(string s,vector<string> vec_str);
    vector<string> string_to_vec_str(string line);
    vector<int> string_to_vec_int(string line);
    static bool make_path(string directory);
public:
    TableMetaTrans(std::string idb,std::string itable,std::vector<FieldMetaTrans> ifts):db(idb),table(itable),fts(ifts){}
    TableMetaTrans(){}
    void set_db(std::string idb){db=idb;}
    string get_db(){return db;}

    void set_table(string itable){table=itable;}
    string get_table(){return table;}
    std::vector<FieldMetaTrans> getFts(){return fts;}

    void set_db_table(string idb,string itable){db=idb;table=itable;}
    void serialize(std::string filename="metadata.data", std::string prefix="data/");
    void deserialize(std::string filename="metadata.data", std::string prefix="data/");
};





