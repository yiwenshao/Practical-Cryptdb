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
#include <algorithm>
#include "wrapper/reuse.hh"
#include "wrapper/common.hh"
#include "wrapper/insert_lib.hh"
#include "util/constants.hh"
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
void show(std::string s){
    create_embedded_thd(0);
    Item* res = MySQLFieldTypeToItem( static_cast<enum_field_types>(253), s);
    (void)res;
    (void)(res->name);
    //able to get the escapsed string
    String s2;
    const_cast<Item &>(*res).print(&s2, QT_ORDINARY);
    (void)(s2.ptr());

    std::string ress(s2.ptr(),s2.length());
    std::cout<<ress<<std::endl;
    return ;
}


int
main(int argc, char* argv[]){
    init();
    create_embedded_thd(0);

    std::string db="tdb",table="student";
    std::string ip="localhost";
    if(argc==4){
        ip = std::string(argv[1]);
        db = std::string(argv[2]);
        table = std::string(argv[3]);
    }
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo(embeddedDir);
    schema.get();
    const std::unique_ptr<AES_KEY> &TK = std::unique_ptr<AES_KEY>(getKey(std::string("113341234")));
    Analysis analysis(db, *schema, TK, SECURITY_RATING::SENSITIVE);
    (void)analysis;

    std::string s("a\nb\nd\0\0",7);
    std::string s2("a\\nb\\nd\\0\\0",11);

    show(s);
    show(s2);
    return 0;
}

