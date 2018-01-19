#pragma once
#include <iostream>
#include <vector>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/CryptoHandlers.hh>
#include <main/rewrite_main.hh>

Item *
my_encrypt_item_layers(const Item &i, onion o, const OnionMeta &om,const Analysis &a, uint64_t IV);


std::ostream&
simple_insert(std::ostream &out, LEX &lex);

std::string
convert_insert(const LEX &lex);

void
my_typical_rewrite_insert_type(const Item &i, const FieldMeta &fm, Analysis &a, std::vector<Item *> *l);


void myRewriteInsertHelper(const Item &i, const FieldMeta &fm, Analysis&a,List<Item> *const append_list);
