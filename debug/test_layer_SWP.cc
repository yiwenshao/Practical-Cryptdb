#include "parser/sql_utils.hh"
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "main/CryptoHandlers.hh"
#include "wrapper/reuse.hh"
#include "wrapper/common.hh"
#include "wrapper/insert_lib.hh"
#include "util/constants.hh"
#include "util/timer.hh"
#include "util/log.hh"
#include "util/onions.hh"

using std::cout;
using std::cin;
using std::endl;
using std::vector;
using std::string;
using std::to_string;
static std::string embeddedDir="/t/cryt/shadow";

static std::map<std::string, WrapperState*> clients;

//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;


/*for each field, convert the format to FieldMeta_Wrapper*/


static void init(){
    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();
    //Connect phase
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    const std::string master_key = "113341234";
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    SharedProxyState *shared_ps =
                        new SharedProxyState(ci, embeddedDir , master_key,
                                            determineSecurityRating());
    assert(0 == mysql_thread_init());
    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    clients[client]->ps->safeCreateEmbeddedTHD();
    //Connect end!!
    globalConn = new Connect(ci.server, ci.user, ci.passwd, ci.port);
}


static
Item *
getItemString(std::string input) {
    return MySQLFieldTypeToItem(MYSQL_TYPE_STRING, input);
}


/*
static
Item *
getItemInt(std::string input) {
    return  new (current_thd->mem_root)
                                Item_int(static_cast<ulonglong>(valFromStr(input)));
}

static
Item *
getItemString(std::string input) {
    return MySQLFieldTypeToItem(MYSQL_TYPE_STRING, input);
}

static
Create_field* getStringField(int length) {
    Create_field *f = new Create_field;
    f->sql_type = MYSQL_TYPE_VARCHAR;
    f->length = length;
    return f;
}

static 
Create_field* getUnsignedIntField(){
    Create_field *f = new Create_field;
    f->sql_type = MYSQL_TYPE_LONG;
    f->flags |= UNSIGNED_FLAG;
    return f;
}

*/

int
main(int argc,char**argv) {
    init();
    create_embedded_thd(0);
    std::string key = "key";
    Create_field *cf = NULL;
    Search* sw = new Search(*cf, key);

    int num_of_tests = 10000;
    int length = 16;

    if(argc==3){
        num_of_tests = std::stoi(std::string(argv[1]));
        length = std::stoi(std::string(argv[2]));
    }else{
        std::cout<<"num_of_tests:length"<<std::endl;
        return 0;
    }

    std::string input(length,'a');

    Item* plain = getItemString(input);

    std::cout<<"length: "<<length<<" ## "<<"num_of_tests: "<<num_of_tests<<std::endl;
    timer t;
    Item* enc = NULL;
    for(int i=0;i<num_of_tests;i++) {
        enc = sw->encrypt(*plain,0u);
    }
    std::cout<<"ENC_OPE_STR_IN_us: "<<t.lap()*1.0/num_of_tests<<std::endl;

    std::cout<<"enclen: "<<enc->str_value.length()<<"##"<<"plainlen: "<<plain->str_value.length() <<std::endl;
    (void)enc;
    return 0;
}

//main/schema.cc:83 is use to create layers of encryption
