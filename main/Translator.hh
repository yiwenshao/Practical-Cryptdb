#pragma once

/*
 * Translator.h
 *
 *  Created on: Aug 13, 2010
 *      Author: raluca
 *
 * Logic of translation between unencrypted and encrypted fields and
 * manipulations of fields and tables.
 */

#include <map>
#include <vector>
#include <list>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#include <main/Connect.hh>
#include <util/onions.hh>

bool isCommand(std::string str);

//it is made to point to the first field after select and distinct if they
// exist
std::string getFieldsItSelect(std::list<std::string> & words, std::list<std::string>::iterator & it);

bool isNested(const std::string &query);

//returns true if token is of the form 'string.string"

bool isField(std::string token);

//returns true if "id" is the name of salt; isTableSalt set to true  if it is
std::string
getTableSalt(std::string anonTableName);
//returns the anonymized name of the table with this salt
std::string getTableOfSalt(std::string salt_name);

//returns the name of the given field as it should appear in the query result
// table, field are unanonymized names
//should allow *
//does not consider field aliases
std::string fieldNameForResponse(std::string table, std::string field,
                                 std::string origName, QueryMeta & qm,
                                 bool isAgg = false);

// fetches the next auto increment value for fullname and updates autoInc
std::string nextAutoInc(std::map<std::string, unsigned int> & autoInc,
                        std::string fullname);

std::string getpRandomName();
