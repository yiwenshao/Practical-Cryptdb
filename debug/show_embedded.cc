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
static void init_embedded_db(){
    std::string client="192.168.1.1:1234";
    ConnectionInfo ci("localhost", "root", "letmein",3306);
    const std::string master_key = "113341234";
    SharedProxyState *shared_ps = new SharedProxyState(ci, embeddedDir , master_key, determineSecurityRating());
    (void)shared_ps;
}


static
void get_and_show(Connect* conn,std::string query){
    std::unique_ptr<DBResult> dbres;
    conn->execute(query,&dbres);
    dbres->showResults(); 
}

int
main() {
    char *buffer;
    if((buffer = getcwd(NULL, 0)) == NULL){
        perror("getcwd error");
    }    
    embeddedDir = std::string(buffer)+"/shadow";//init embedded db
    init_embedded_db();
    std::unique_ptr<Connect> e_conn(Connect::getEmbedded(embeddedDir));
    std::string query;
    while(true){
        std::getline(std::cin,query);
        if(query!=std::string("q"))
            get_and_show(e_conn.get(),query);
        else break;
    }
    return 0;
}
