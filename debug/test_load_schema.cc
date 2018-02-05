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
using std::cout;
using std::endl;


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

static void processOnionMeta(const OnionMeta &om){
    for(auto &item:om.getLayers()){
        cout<<" "<<TypeText<SECLEVEL>::toText(item->level())<<" ";
    }
    cout<<endl;
}


static void processFieldMeta(const FieldMeta &field){
    std::cout<<GREEN_BEGIN<<"PRINT FieldMeta"<<COLOR_END<<std::endl;
    for(const auto & onionnow: field.getChildren()){
        std::cout<<"onionmetaid: "<<onionnow.second->getDatabaseID()
                 <<" ### onion name: "<<TypeText<onion>::toText(onionnow.first.getValue())<<std::endl;
        processOnionMeta(*(onionnow.second));
    }
    std::cout<<GREEN_BEGIN<<"end FieldMeta"<<COLOR_END<<std::endl;
}

static void processTableMeta(const TableMeta &table){
    std::cout<<GREEN_BEGIN<<"PRINT TableMeta"<<COLOR_END<<std::endl;
    for(const auto & field: table.getChildren()){
        std::cout<<"fieldmetaid: "<<field.second->getDatabaseID()
                 <<" ### fieldmeta name: "<<field.first.getValue()<<std::endl;
        processFieldMeta(*(field.second));
    }
}

static void processDatabaseMeta(const DatabaseMeta & db) {
    std::cout<<GREEN_BEGIN<<"PRINT DatabaseMeta"<<COLOR_END<<std::endl;
    for(const auto & table: db.getChildren()){
        //cout<<"tablemetaid: "<<table.second->getDatabaseID()
        //cout<<" ### tablemeta name: "<<table.first.getValue()<<
        //"annotablename: "<<table.second->getAnonTableName()<<std::endl;
        cout<<"tablemap: "<<table.second->getAnonTableName()<<" "<<table.first.getValue()<<std::endl;
        processTableMeta(*(table.second));
    }
}

static void processSchemaInfo(SchemaInfo &schema){
    //we have a map here
     std::cout<<GREEN_BEGIN<<"PRINT SchemaInfo"<<COLOR_END<<std::endl;
    //only const auto & is allowed, now copying. or we meet use of deleted function.
    for(const auto & child : schema.getChildren()) {
        std::cout<<"dbmetaid: "<<child.second->getDatabaseID()<<" ### dbmeta name:"<<child.first.getValue()<<std::endl;
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

static 
std::unique_ptr<SchemaInfo> myLoadSchemaInfo(){
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::unique_ptr<SchemaInfo> schema(new SchemaInfo());
    loadChildren(schema.get(),e_conn);
    return schema;
}

static void init_embedded_db(){
    std::string client="192.168.1.1:1234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    const std::string master_key = "113341234";
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    (void)shared_ps;
}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }    
    embeddedDir = std::string(buffer)+"/shadow";//init embedded db
    init_embedded_db();

    std::unique_ptr<SchemaInfo> sm = myLoadSchemaInfo();
    processSchemaInfo(*sm);
    return 0;
}
