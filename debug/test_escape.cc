#include "wrapper/common.hh"
#include "wrapper/reuse.hh"
#include "util/util.hh"
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


int
main(int argc, char* argv[]){    
    std::string db="tdb",table="student";
    std::string ip="127.0.0.1";
    int port=3306;
    if(argc==2){
        ip = std::string(argv[1]);
    }
    init(ip,port);
//    std::string es("a\0\nb\\n",6);
//   std::cout<<es<<std::endl;
//    std::unique_ptr<Connect> con(globalConn);
//    std::string res = escapeString(con,es);
//    char * buf = new char[100];
//    char * buf2 = new char[100];
//    escape_string_for_mysql_modify(buf,es.c_str(),es.size());
//    printf("%s\n",buf);
//    reverse_escape_string_for_mysql_modify(buf2,buf);
//    printf("%s",buf2);

    char * original = new char[1024];
    for(int i=0;i<1023;i++){
        original[i]=i;
    }
    original[1023]='\0';
    std::cout<<strlen(original)<<std::endl;
    std::string oriStr(original,1023);
    std::cout<<oriStr<<std::endl;
    std::cout<<strlen(original)<<std::endl;

    std::unique_ptr<Connect> con(globalConn);
    std::string esp_cryptdb = escapeString(con,oriStr);
    
    //reverse the string escaped by cryptdb.
    char* reverse = new char[2048];
    size_t len = reverse_escape_string_for_mysql_modify(reverse,esp_cryptdb.c_str());
    std::string back(reverse,len);
    assert(back==oriStr);

    char* esp_modify = new char[2048];
    len = escape_string_for_mysql_modify(esp_modify,original,1023);
    std::string esp_my(esp_modify,len);
    assert(esp_my==esp_cryptdb);

    return 0;
}


