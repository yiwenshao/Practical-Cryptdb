/*
used to check to schema info of all the current databases

*/

#include <cstdlib>
#include <cstdio>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <main/Connect.hh>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>
#include <main/CryptoHandlers.hh>

#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
#include <parser/lex_util.hh>

#include <readline/readline.h>
#include <readline/history.h>

#include <crypto/ecjoin.hh>
#include <util/errstream.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <util/yield.hpp>
#include "util/onions.hh"


#include <sstream>
#include <map>

std::map<SECLEVEL,std::string> gmp;



static std::string embeddedDir="/t/cryt/shadow";


static void processLayers(const EncLayer &enc){
    //std::cout<<enc.serialize(enc)<<std::endl;
    std::cout<<enc.name()<<std::endl;
}



static void processOnionMeta(const OnionMeta &onion,std::string pre="\t\t\t"){
    //std::cout<<GREEN_BEGIN<<"PRINT OnionMeta"<<COLOR_END<<std::endl;
    //std::cout<<"onionmeta->getAnonOnionName(): "<<onion.getAnonOnionName()<<std::endl; 
    auto &layers = onion.getLayers();
    for(auto &slayer:layers){
	std::cout<<pre;        
        processLayers(*(slayer.get()));
    }
}

static void processFieldMeta(const FieldMeta &field,std::string pre="\t\t"){
    //std::cout<<GREEN_BEGIN<<"PRINT FieldMeta"<<COLOR_END<<std::endl;
    for(const auto & onion: field.getChildren()){
	std::cout<<pre;
        //std::cout<<onion.second->getDatabaseID()<<":"<<onion.first.getValue()<<std::endl;
        std::cout<<onion.second->getAnonOnionName()<<std::endl;
        processOnionMeta(*(onion.second));
    }
}

static void processTableMeta(const TableMeta &table,std::string pre="\t"){
    //std::cout<<GREEN_BEGIN<<"PRINT TableMeta"<<COLOR_END<<std::endl;
    for(const auto & field: table.getChildren()){
	std::cout<<pre;
          //std::cout<<"fieldmeta->stringify: "<<field.second->stringify()<<std::endl; 
        std::cout<<field.second->getFieldName()<<std::endl;
        //std::cout<<field.second->getDatabaseID()<<":"<<field.first.getValue()<<std::endl;
        processFieldMeta(*(field.second));
    }
}


static void processDatabaseMeta(const DatabaseMeta & db,std::string pre = "") {
    //std::cout<<GREEN_BEGIN<<"PRINT DatabaseMeta"<<COLOR_END<<std::endl;
    for(const auto & table: db.getChildren()){
	std::cout<<pre;
        //std::cout<<table.second->getDatabaseID()<<":"<<table.first.getValue()<<":"<<
        //    table.first.getSerial()<<std::endl;
        std::cout<<"tableMeta->getAnonTableName: "<<table.second->getAnonTableName()<<" : "<<table.first.getValue() <<std::endl;
        processTableMeta(*(table.second));
    }
}



static void processSchemaInfo(SchemaInfo &schema,std::string pre = ""){
    //we have a map here
     //std::cout<<GREEN_BEGIN<<"PRINT SchemaInfo"<<COLOR_END<<std::endl;
    //only const auto & is allowed, now copying. or we meet use of deleted function.
    for(const auto & child : schema.getChildren()) {
        //std::cout<<child.second->getDatabaseID()<<":"<<child.first.getValue()<<":"<<
        //    child.first.getSerial()<<std::endl;
        //std::cout<<GREEN_BEGIN<<child.first.getValue()<<COLOR_END<<std::endl;
        std::cout<<child.first.getValue()<<std::endl;
        processDatabaseMeta(*(child.second));
    }
}

static std::unique_ptr<SchemaInfo> myLoadSchemaInfo() {
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::unique_ptr<SchemaInfo> schema(new SchemaInfo());

    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }
            return parent;
        };
    //load all metadata and then store it in schema
    loadChildren(schema.get());
    //Analysis analysis(std::string("student"),*schema,std::unique_ptr<AES_KEY>(getKey(std::string("113341234"))),
    //                    SECURITY_RATING::SENSITIVE);
    //test_Analysis(analysis);
    return schema;
}

int
main() {

    gmp[SECLEVEL::INVALID]="INVALID";
    gmp[SECLEVEL::PLAINVAL]="PLAINVAL";
    gmp[SECLEVEL::OPE]="OPE";
    gmp[SECLEVEL::DETJOIN]="DETJOIN";
    gmp[SECLEVEL::OPEFOREIGN]="OPEFOREIGN";
    gmp[SECLEVEL::DET]="DET";
    gmp[SECLEVEL::SEARCH]="SEARCH";
    gmp[SECLEVEL::HOM]="HOM";
    gmp[SECLEVEL::RND]="RND";

    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";

    const std::string master_key = "113341234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(shared_ps!=NULL);
    std::unique_ptr<SchemaInfo> schema =  myLoadSchemaInfo();
    processSchemaInfo(*schema);
    return 0;
}
