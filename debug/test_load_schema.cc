#include <map>
#include <iostream>
#include <vector>
#include <set>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <main/Connect.hh>
#include <main/Analysis.hh>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>

static std::string embeddedDir="/t/cryt/shadow";

//My WrapperState.
class WrapperState {
    WrapperState(const WrapperState &other);
    WrapperState &operator=(const WrapperState &rhs);
    KillZone kill_zone;
public:
    std::string last_query;
    std::string default_db;

    WrapperState() {}
    ~WrapperState() {}
    const std::unique_ptr<QueryRewrite> &getQueryRewrite() const {
        assert(this->qr);
        return this->qr;
    }
    void setQueryRewrite(std::unique_ptr<QueryRewrite> &&in_qr) {
        this->qr = std::move(in_qr);
    }
    void selfKill(KillZone::Where where) {
        kill_zone.die(where);
    }
    void setKillZone(const KillZone &kz) {
        kill_zone = kz;
    }
    
    std::unique_ptr<ProxyState> ps;
    std::vector<SchemaInfoRef> schema_info_refs;

private:
    std::unique_ptr<QueryRewrite> qr;
};

//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;

//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;

static void processFieldMeta(const FieldMeta &field){
    std::cout<<GREEN_BEGIN<<"PRINT FieldMeta"<<COLOR_END<<std::endl;
    for(const auto & onion: field.getChildren()){
        std::cout<<onion.second->getDatabaseID()<<":"<<onion.first.getValue()<<std::endl;
    }
    std::cout<<GREEN_BEGIN<<"end FieldMeta"<<COLOR_END<<std::endl;
}

static void processTableMeta(const TableMeta &table){
    std::cout<<GREEN_BEGIN<<"PRINT TableMeta"<<COLOR_END<<std::endl;
    for(const auto & field: table.getChildren()){
        std::cout<<field.second->getDatabaseID()<<":"<<field.first.getValue()<<std::endl;
        processFieldMeta(*(field.second));
    }
}

static void processDatabaseMeta(const DatabaseMeta & db) {
    std::cout<<GREEN_BEGIN<<"PRINT DatabaseMeta"<<COLOR_END<<std::endl;
    for(const auto & table: db.getChildren()){
        processTableMeta(*(table.second));
    }
}

static void processSchemaInfo(SchemaInfo &schema){
    //we have a map here
     std::cout<<GREEN_BEGIN<<"PRINT SchemaInfo"<<COLOR_END<<std::endl;
    //only const auto & is allowed, now copying. or we meet use of deleted function.
    for(const auto & child : schema.getChildren()) {
        std::cout<<child.second->getDatabaseID()<<":"<<child.first.getValue()<<std::endl;
        processDatabaseMeta(*(child.second));
    }
}

static DBMeta* loadChildren(DBMeta *const parent,std::unique_ptr<Connect> &e_conn){
    auto kids = parent->fetchChildren(e_conn);
    for (auto it : kids) {
        loadChildren(it,e_conn);
    }
    return parent;
}

static std::unique_ptr<SchemaInfo> myLoadSchemaInfo(){
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::unique_ptr<SchemaInfo> schema(new SchemaInfo());

    loadChildren(schema.get(),e_conn);
//    auto load = std::bind(loadChildren,std::placeholders::_1,e_conn);
/*    std::function<DBMeta *(DBMeta *const)> loadChildren =
        [&loadChildren, &e_conn](DBMeta *const parent) {
            auto kids = parent->fetchChildren(e_conn);
            for (auto it : kids) {
                loadChildren(it);
            }
            return parent;
        };*/
    //load all metadata and then store it in schema
    //loadChildren(schema.get());
//    load(schema.get());

    //Analysis analysis(std::string("student"),*schema,std::unique_ptr<AES_KEY>(getKey(std::string("113341234"))),
    //                    SECURITY_RATING::SENSITIVE);
    return schema;
}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }
    embeddedDir = std::string(buffer)+"/shadow";
    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();    
    //Connect phase
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    const std::string master_key = "113341234";
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    assert(0 == mysql_thread_init());
    //we init embedded database here.
    clients[client]->ps = std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    clients[client]->ps->safeCreateEmbeddedTHD();
    //Connect end!!
    std::unique_ptr<SchemaInfo> sm = myLoadSchemaInfo();
    processSchemaInfo(*sm);
    return 0;
}
