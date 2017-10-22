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

#include <sstream>
#include <unistd.h>
#include <map>


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



struct big_proxy{
    Connect  *globalConn;
    std::map<std::string, WrapperState*> clients;
    std::string curQuery;
    std::string targetDb;
    unsigned long long _thread_id;
    std::string client;
    std::string embeddedDir="/t/cryt/shadow";

    big_proxy(std::string db = "tdb");
    void myNext(std::string client,bool isFirst,ResType inRes);
    void batchTogether(std::string client, std::string curQuery,unsigned long long _thread_id);
    bool myRewrite(std::string curQuery,unsigned long long _thread_id,std::string client);
    void go(std::string query);   
};
