#include "wrapper/common.hh"
#include "wrapper/reuse.hh"
static std::string embeddedDir="/t/cryt/shadow";
//global map, for each client, we have one WrapperState which contains ProxyState.
static std::map<std::string, WrapperState*> clients;
//This connection mimics the behaviour of MySQL-Proxy
Connect  *globalConn;

static void init(std::string ip,int port){
    std::string client="192.168.1.1:1234";
    //one Wrapper per user.
    clients[client] = new WrapperState();    
    //Connect phase
    ConnectionInfo ci("localhost", "root", "letmein",port);
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
    globalConn = new Connect(ip, ci.user, ci.passwd, port);
}

static
bool local_make_path(string directory){
    struct stat st;
    if(directory.size()==0||directory[0]=='/') return false;
    if(directory.back()=='/') directory.pop_back();
    int start = 0,next=0; 
    while(stat(directory.c_str(),&st)==-1&&next!=-1){
        next = directory.find('/',start);
        if(next!=-1){
            string sub = directory.substr(0,next);
            if(stat(sub.c_str(),&st)==-1)
                mkdir(sub.c_str(),0700);
            start =  next + 1;
        }else{
            mkdir(directory.c_str(),0700);
        }
    }
    return true;
}


static
void write_raw_data_to_files(MySQLColumnData& resraw,string db,string table,std::string dir){
    //write datafiles
    std::string prefix = dir+"/" +db+"/"+table+"/";
    local_make_path(prefix);
    std::vector<std::string> filenames;
    for(auto item:resraw.fieldNames){
        item=prefix+item;
        filenames.push_back(item);
    }
    int len = resraw.fieldNames.size();

    for(int i=0;i<len;i++){
        if(IS_NUM(resraw.fieldTypes[i])){
            writeColumndataNum(resraw.columnData[i],filenames[i]);
        }else{
            writeColumndataEscapeString(resraw.columnData[i],filenames[i],resraw.maxLengths[i]);
        }
    }
}

static
std::vector<std::string>
getTables(std::string db) {
    std::string query = std::string("show tables in ")+db;
    MySQLColumnData resraw =  executeAndGetColumnData(globalConn,query);
    return resraw.columnData[0];
}


static void store(std::string db, std::string table,std::string dir){
    std::string backup_query = std::string("SELECT * FROM `")+db+"`.`"+table+"`";
    MySQLColumnData resraw =  executeAndGetColumnData(globalConn,backup_query);
    //write the tuples into files
    write_raw_data_to_files(resraw,db,table,dir);
}

int
main(int argc, char* argv[]){    
    std::string db="tdb";
    std::string ip="127.0.0.1";
    std::string dir="onlyfields";
    int port=3306;
    if(argc==5){
        ip = std::string(argv[1]);
        db = std::string(argv[2]);
        dir = std::string(argv[3]);
    }else{
        std::cout<<"need ip, db, dir"<<std::endl;
        return 0;
    }
    init(ip,port);
    auto tables = getTables(db);
    for(auto table:tables){
        store(db,table,dir);
    }
    return 0;
}
