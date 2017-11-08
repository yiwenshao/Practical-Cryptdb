#include <sstream>
#include <fstream>
#include <assert.h>
#include <lua5.1/lua.hpp>

#include <util/ctr.hh>
#include <util/cryptdb_log.hh>
#include <util/scoped_lock.hh>
#include <util/util.hh>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/schema.hh>
#include <main/Analysis.hh>

#include <parser/sql_utils.hh>
#include <parser/mysql_type_metadata.hh>

//thread local variable
__thread ProxyState *thread_ps = NULL;

//wrapperstate contains proxystate. one per client.
class WrapperState{
    WrapperState(const WrapperState &other);
    WrapperState &operator=(const WrapperState &rhs);
    KillZone kill_zone;
public:
    std::string last_query;
    std::string default_db;
    std::ofstream * PLAIN_LOG;
    WrapperState() {}
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

//commented
//static Timer t;

static SharedProxyState * shared_ps = NULL;

//this ensures that only one client can call connect or next
static pthread_mutex_t big_lock;

static bool EXECUTE_QUERIES = true;

static std::string TRAIN_QUERY ="";


static std::string PLAIN_BASELOG = "";



static std::map<std::string, WrapperState*> clients;

static void
returnResultSet(lua_State *L, const ResType &res);

static Item_null *
make_null(const std::string &name = ""){
    char *const n = current_thd->strdup(name.c_str());
    return new Item_null(n);
}

static std::string
xlua_tolstring(lua_State *const l, int index){
    size_t len;
    char const *const s = lua_tolstring(l, index, &len);
    return std::string(s, len);
}

static void
xlua_pushlstring(lua_State *const l, const std::string &s){
    lua_pushlstring(l, s.data(), s.length());
}

static int
connect(lua_State *const L) {
//TODO: added, why test here?
    assert(test64bitZZConversions());

//    ANON_REGION(__func__, &perf_cg);
    //Only one client can connect at a time
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());

    //fetch lua paramaters.
    const std::string client = xlua_tolstring(L, 1);
    const std::string server = xlua_tolstring(L, 2);
    const uint port = luaL_checkint(L, 3);
    const std::string user = xlua_tolstring(L, 4);
    const std::string psswd = xlua_tolstring(L, 5);
    const std::string embed_dir = xlua_tolstring(L, 6);

    ConnectionInfo const ci = ConnectionInfo(server, user, psswd, port);

    assert(clients.end() == clients.find(client));
    //one wrapperstate per client. This is deleted when the client leaves
    clients[client] = new WrapperState();

    /*shared_ps is created as the first client comes in, and it is preserved.
    * each proxystate takes a const reference of the sharedproxy state, which
    * contains the schemainfo
    */
    if (!shared_ps) {
        const std::string &mkey      = "113341234";
        shared_ps =
            new SharedProxyState(ci, embed_dir, mkey,
                                 determineSecurityRating());
    }
    clients[client]->ps =
        std::unique_ptr<ProxyState>(new ProxyState(*shared_ps));
    // We don't want to use the THD from the previous connection
    // if such is even possible...
    clients[client]->ps->safeCreateEmbeddedTHD();
    return 0;
}

static int
disconnect(lua_State *const L) {
    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());
    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        return 0;
    }
    LOG(wrapper) << "disconnect " << client;
    auto ws = clients[client];
    clients[client] = NULL;
    thread_ps = NULL;
    delete ws;
    clients.erase(client);
    mysql_thread_end();
    return 0;
}

static int
rewrite(lua_State *const L) {
//    ANON_REGION(__func__, &perf_cg);
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());
         
    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        lua_pushnil(L);
        xlua_pushlstring(L, "failed to recognize client");     
        return 2;
    }

    WrapperState *const c_wrapper = clients[client];
    ProxyState *const ps = thread_ps = c_wrapper->ps.get();
    assert(ps);

    const std::string &query = xlua_tolstring(L, 2);
    const unsigned long long _thread_id =
        strtoull(xlua_tolstring(L, 3).c_str(), NULL, 10);
    //this is not used??
    c_wrapper->last_query = query;
    if (EXECUTE_QUERIES) {
        try {
            TEST_Text(retrieveDefaultDatabase(_thread_id, ps->getConn(),
                                              &c_wrapper->default_db),
                      "proxy failed to retrieve default database!");
            // save a reference so a second thread won't eat objects
            // that DeltaOuput wants later
            const std::shared_ptr<const SchemaInfo> &schema =
                ps->getSchemaInfo();
            c_wrapper->schema_info_refs.push_back(schema);

            //parse, rewrite, delta, adjust, returnMeta, 
            std::unique_ptr<QueryRewrite> qr =
                std::unique_ptr<QueryRewrite>(new QueryRewrite(
                    Rewriter::rewrite(query, *schema.get(),
                                      c_wrapper->default_db, *ps)));
            assert(qr);
            c_wrapper->setQueryRewrite(std::move(qr));
        } catch (const AbstractException &e) {
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.to_string());     // error message
            return 2;
        } catch (const CryptDBError &e) {
            lua_pushboolean(L, false);              // status
            xlua_pushlstring(L, e.msg);             // error message
            return 2;
        }
    }
    lua_pushboolean(L, true);                       // status
    lua_pushnil(L);                                 // error message
    return 2;
}

inline std::vector<Item *>
itemNullVector(unsigned int count)
{
    std::vector<Item *> out;
    for (unsigned int i = 0; i < count; ++i) {
        out.push_back(make_null());
    }

    return out;
}

struct rawReturnValue{
    std::vector<std::vector<std::string> > rowValues;
    std::vector<std::string> fieldNames;
    std::vector<int> fieldTypes;
};

static ResType
getResTypeFromLuaTable(lua_State *const L, int fields_index,
                       int rows_index, int affected_rows_index,
                       int insert_id_index, int status_index) {
    const bool status = lua_toboolean(L, status_index);
    if (false == status) {
        return ResType(false, 0, 0);
    }

    rawReturnValue myRawFromLua;

    std::vector<std::string> names;
    std::vector<enum_field_types> types;
    /* iterate over the fields argument */
    lua_pushnil(L);
    while (lua_next(L, fields_index)) {
        if (!lua_istable(L, -1))
            LOG(warn) << "mismatch";
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const std::string k = xlua_tolstring(L, -2);
            if ("name" == k) {
                names.push_back(xlua_tolstring(L, -1));
                myRawFromLua.fieldNames.push_back(xlua_tolstring(L, -1));
            } else if ("type" == k) {
                types.push_back(static_cast<enum_field_types>(luaL_checkint(L, -1)));
                myRawFromLua.fieldTypes.push_back(static_cast<enum_field_types>(luaL_checkint(L, -1)) );
            } else {
                LOG(warn) << "unknown key " << k;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    assert(names.size() == types.size());

    /* iterate over the rows argument */
    std::vector<std::vector<Item *> > rows;
    lua_pushnil(L);

    //没有kv对了, 则退出.
    while (lua_next(L, rows_index)) {
        if (!lua_istable(L, -1))
            LOG(warn) << "mismatch";

        /* initialize all items to NULL, since Lua skips
           nil array entries */
        std::vector<Item *> row = itemNullVector(types.size());
        std::vector<std::string> curRow;
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            const int key = luaL_checkint(L, -2) - 1;
            assert(key >= 0 && static_cast<uint>(key) < types.size());
            const std::string data = xlua_tolstring(L, -1);
            curRow.push_back(data);
            row[key] = MySQLFieldTypeToItem(types[key], data);
            lua_pop(L, 1);
        }
        rows.push_back(row);
        myRawFromLua.rowValues.push_back(curRow);
        lua_pop(L, 1);

    }
    //printrawReturnValue(myRawFromLua);

    return ResType(status, lua_tointeger(L, affected_rows_index),
                   lua_tointeger(L, insert_id_index), std::move(names),
                   std::move(types), std::move(rows));
}

static void
nilBuffer(lua_State *const L, size_t count)
{
    while (count--) {
        lua_pushnil(L);
    }

    return;
}

/*
 *return mete for dectypting data.
 * */
static void 
parseReturnMeta(const ReturnMeta & rtm){
}


static int
next(lua_State *const L) {
    scoped_lock l(&big_lock);
    assert(0 == mysql_thread_init());
    //查找client
    const std::string client = xlua_tolstring(L, 1);
    if (clients.find(client) == clients.end()) {
        xlua_pushlstring(L, "error");
        xlua_pushlstring(L, "unknown client");
         lua_pushinteger(L,  100);
        xlua_pushlstring(L, "12345");

        nilBuffer(L, 1);
        return 5;
    }

    WrapperState *const c_wrapper = clients[client];

    assert(EXECUTE_QUERIES);

    ProxyState *const ps = thread_ps = c_wrapper->ps.get();
    assert(ps);
    ps->safeCreateEmbeddedTHD();

    const ResType &res = getResTypeFromLuaTable(L, 2, 3, 4, 5, 6);


    const std::unique_ptr<QueryRewrite> &qr = c_wrapper->getQueryRewrite();
    parseReturnMeta(qr->rmeta);
    try {
        NextParams nparams(*ps, c_wrapper->default_db, c_wrapper->last_query);

        c_wrapper->selfKill(KillZone::Where::Before);
        const auto &new_results = qr->executor->next(res, nparams);
        c_wrapper->selfKill(KillZone::Where::After);

        const auto &result_type = new_results.first;
        if (result_type != AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN) {
            // set the killzone when we are done with this query
            // > a given killzone will only apply to the next query translation
            c_wrapper->setKillZone(qr->kill_zone);
        }
        switch (result_type) {
        case AbstractQueryExecutor::ResultType::QUERY_COME_AGAIN: {
            // more to do before we have the client's results
            xlua_pushlstring(L, "again");
            const auto &output =
                std::get<1>(new_results)->extract<std::pair<bool, std::string> >();
            const auto &want_interim = output.first;
            lua_pushboolean(L, want_interim);
            const auto &next_query = output.second;
            xlua_pushlstring(L, next_query);
            nilBuffer(L, 2);
            return 5;
        }
        case AbstractQueryExecutor::ResultType::QUERY_USE_RESULTS: {
            // the results of executing this query should be send directly
            // back to the client
            xlua_pushlstring(L, "query-results");
            const auto &new_query =
                std::get<1>(new_results)->extract<std::string>();
            xlua_pushlstring(L, new_query);
            nilBuffer(L, 3);
            return 5;
        }
        case AbstractQueryExecutor::ResultType::RESULTS: {
            // ready to return results to the client
            xlua_pushlstring(L, "results");
            const auto &res = new_results.second->extract<ResType>();
            returnResultSet(L, res);        // pushes 4 items on stack
            return 5;
        }
        default:
            assert(false);
        }
    } catch (const ErrorPacketException &e) {
        // lua_pop(L, lua_gettop(L));
        xlua_pushlstring(L, "error");
        xlua_pushlstring(L, e.getMessage());
         lua_pushinteger(L, e.getErrorCode());
        xlua_pushlstring(L, e.getSQLState());

        nilBuffer(L, 1);
        return 5;
    }
}

static void
returnResultSet(lua_State *const L, const ResType &rd) {
    TEST_GenericPacketException(true == rd.ok, "something bad happened");

    lua_pushinteger(L, rd.affected_rows);
    lua_pushinteger(L, rd.insert_id);

    /* return decrypted result set */
    lua_createtable(L, (int)rd.names.size(), 0);
    int const t_fields = lua_gettop(L);

    for (uint i = 0; i < rd.names.size(); i++) {
        lua_createtable(L, 0, 1);
        int const t_field = lua_gettop(L);

        /* set name for field */
        xlua_pushlstring(L, rd.names[i]);       // plaintext fields
        lua_setfield(L, t_field, "name");
        /* insert field element into fields table at i+1 */
        lua_rawseti(L, t_fields, i+1);
    }

    lua_createtable(L, static_cast<int>(rd.rows.size()), 0);
    int const t_rows = lua_gettop(L);
    for (uint i = 0; i < rd.rows.size(); i++) {
        lua_createtable(L, static_cast<int>(rd.rows[i].size()), 0);
        int const t_row = lua_gettop(L);

        for (uint j = 0; j < rd.rows[i].size(); j++) {
            if (NULL == rd.rows[i][j]) {
                lua_pushnil(L);                 // plaintext rows
            } else {
                xlua_pushlstring(L,             // plaintext rows
                                 ItemToString(*rd.rows[i][j]));
            }
            lua_rawseti(L, t_row, j+1);
        }

        lua_rawseti(L, t_rows, i+1);
    }

    return;
}

static const struct luaL_reg
cryptdb_lib[] = {
#define F(n) { #n, n }
    F(connect),
    F(disconnect),
    F(rewrite),
    F(next),
    { 0, 0 },
#undef F
};

extern "C" int lua_cryptdb_init(lua_State * L);

int
lua_cryptdb_init(lua_State *const L) {
    luaL_openlib(L, "CryptDB", cryptdb_lib, 0);
    return 1;
}
