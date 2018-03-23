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
    free(buffer);
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
getItemInt(std::string input) {
    return  new (current_thd->mem_root)
                                Item_int(static_cast<ulonglong>(valFromStr(input)));
}

static
Item *
getItemIntNew(std::string input){
    return ::new Item_int(static_cast<ulonglong>(valFromStr(input)));
}

static 
Create_field* getUnsignedIntField(){
    Create_field *f = new Create_field;
    f->sql_type = MYSQL_TYPE_LONG;
    f->flags |= UNSIGNED_FLAG;
    return f;
}

int
main(int argc, char**argv) {
    UNUSED(getItemInt);
    UNUSED(getItemIntNew);
    UNUSED(getUnsignedIntField);
    //still has 80B memory leak
    init();
    create_embedded_thd(0);
    std::string key = "key";
    Create_field *cf = getUnsignedIntField();
    RND_int* ri = new RND_int(*cf, key);
    Item * plain = ::new Item_int(static_cast<ulonglong>(valFromStr("123456789")));
//    Item * plain = getItemIntNew("123456789");
//    plain = getItemIntNew("1233344");
//    Item * enc = NULL;
//    Item * dec = NULL;
//    enc = ri->encrypt(*plain,0u);
//    dec = ri->decrypt(*enc,0u);
//    delete ri;
//    delete cf;
    UNUSED(plain);
    UNUSED(cf);
    UNUSED(ri);
    return 0;
}
//main/schema.cc:83 is use to create layers of encryption
