#include <main/Analysis.hh>
#include <main/rewrite_util.hh>
#include <main/rewrite_main.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>
#include <main/stored_procedures.hh>
#include <util/util.hh>

// FIXME: Wrong interfaces.
EncSet::EncSet(Analysis &a, FieldMeta * const fm) {
    TEST_TextMessageError(0 != fm->getChildren().size(),
                         "FieldMeta has no children!");
    osl.clear();
    for (const auto &pair : fm->getChildren()) {
        OnionMeta *const om = pair.second.get();
        OnionMetaKey const &key = pair.first;
        //就是当前的onionmeta的back 对应的level
        osl[key.getValue()] = LevelFieldPair(a.getOnionLevel(*om), fm);
    }
}

EncSet::EncSet(const OLK & olk) {
    osl[olk.o] = LevelFieldPair(olk.l, olk.key);
}

EncSet
EncSet::intersect(const EncSet & es2) const
{
    OnionLevelFieldMap m;
    for (const auto &it2 : es2.osl) {
        auto it = osl.find(it2.first);

        if (it != osl.end()) {
            FieldMeta * const fm = it->second.second;
            FieldMeta * const fm2 = it2.second.second;

            const onion o = it->first;
            const onion o2 = it2.first;

            assert(o == o2);

            const SECLEVEL sl =
                static_cast<SECLEVEL>(
                        min(static_cast<int>(it->second.first),
                            static_cast<int>(it2.second.first)));

            /*
             * FIXME: Each clause of this if statement should make sure
             * that it's OnionMeta actually has the SecLevel.
             */
            if (fm == NULL) {
                m[o] = LevelFieldPair(sl, fm2);
            } else if (fm2 == NULL) {
                m[it->first] = LevelFieldPair(sl, fm);
            } else {
                // This can succeed in three cases.
                // 1> Same field, so same key.
                // 2> Different fields, but SECLEVEL is PLAINVAL
                //    or DETJOIN so same key.
                // 3> Differt fields, and SECLEVEL is HOM so
                //    we will do computation client side if necessary.
                const OnionMeta * const om = fm->getOnionMeta(o);
                const OnionMeta * const om2 = fm2->getOnionMeta(o);
                // HACK: To determine if the keys are the same.
                if ((om->hasEncLayer(sl) && om2->hasEncLayer(sl)
                     && om->getLayer(sl)->doSerialize() ==
                        om2->getLayer(sl)->doSerialize())) {
                    m[o] = LevelFieldPair(sl, fm);
                }
            }
        }
    }
    return EncSet(m);
}

std::ostream&
operator<<(std::ostream &out, const EncSet &es)
{
    if (es.osl.size() == 0) {
        out << "empty encset";
    }
    for (auto it : es.osl) {
        out << "(onion " << it.first
            << ", level " << TypeText<SECLEVEL>::toText(it.second.first)
            << ", field `" << (it.second.second == NULL ? "*" : it.second.second->getFieldName()) << "`"
            << ") ";
    }
    return out;
}


OLK
EncSet::chooseOne() const {
    // Order of selection is encoded in this array.
    // The onions appearing earlier are the more preferred ones.
    static const onion onion_order[] = {
        oDET,
        oOPE,
        oAGG,
        oASHE,
        oSWP,
        oPLAIN,
    };

    static size_t onion_size =
        sizeof(onion_order) / sizeof(onion_order[0]);
    for (size_t i = 0; i < onion_size; i++) {
        const onion o = onion_order[i];
        const auto it = osl.find(o);
        if (it != osl.end()) {
            if (SECLEVEL::INVALID == it->second.first) {
                continue;
            }
            if (0 == it->second.second
                && (it->second.first != SECLEVEL::PLAINVAL
                    && o != oPLAIN)) {
                /*
                 * If no key, skip this OLK.
                 */
                continue;
            }

            return OLK(o, it->second.first, it->second.second);
        }
    }

    return OLK::invalidOLK();
}

bool
EncSet::contains(const OLK &olk) const {
    auto it = osl.find(olk.o);
    if (it == osl.end()) {
        return false;
    }
    if (it->second.first == olk.l) {
        return true;
    }
    return false;
}

bool
EncSet::hasSecLevel(SECLEVEL level) const
{
    for (auto it : osl) {
        if (it.second.first == level) {
            return true;
        }
    }

    return false;
}

SECLEVEL
EncSet::onionLevel(onion o) const
{
    for (auto it : osl) {
        if (it.first == o) {
            return it.second.first;
        }
    }

    assert(false);
}

bool
EncSet::available() const
{
    return OLK::isNotInvalid(this->chooseOne());
}

bool EncSet::single_crypted_and_or_plainvals() const
{
    unsigned int crypted = 0;
    unsigned int plain = 0;
    for (auto it : osl) {
        if (SECLEVEL::PLAINVAL == it.second.first) {
            ++plain;
        } else {
            ++crypted;
        }
    }

    return 1 >= crypted || plain > 0;
}

OLK EncSet::extract_singleton() const
{
    assert_s(singleton(), std::string("encset has size ") +
                            StringFromVal(osl.size()));
    const auto it = osl.begin();
    return OLK(it->first, it->second.first, it->second.second);
}

// needsSaltz must have consistent semantics. shaoyiwen
static bool
needsSalt(SECLEVEL l)
{
//    return l == SECLEVEL::RND||l==SECLEVEL::ASHE;
      return l == SECLEVEL::RND;
}

bool
needsSalt(OLK olk)
{
    return olk.key && olk.key->getHasSalt() && needsSalt(olk.l);
}

bool
needsSalt(EncSet es)
{
    for (auto pair : es.osl) {
        OLK olk(pair.first, pair.second.first, pair.second.second);
        if (needsSalt(olk)) {
            return true;
        }
    }

    return false;
}

std::ostream&
operator<<(std::ostream &out, const reason &r)
{
    out << r.string_item << " PRODUCES encset " << r.encset << std::endl
        << " BECAUSE " << r.why << std::endl;

    return out;
}

std::ostream&
operator<<(std::ostream &out, const RewritePlan * const rp)
{
    if (!rp) {
        out << "NULL RewritePlan";
        return out;
    }

    out << " RewritePlan: \n---> out encset " << rp->es_out << "\n---> reason " << rp->r << "\n";

    return out;
}

bool
lowLevelGetCurrentDatabase(const std::unique_ptr<Connect> &c,
                           std::string *const out_db)
{
    const std::string query = "SELECT DATABASE();";
    std::unique_ptr<DBResult> db_res;
    RFIF(c->execute(query, &db_res));
    assert(1 == mysql_num_rows(db_res->n));

    const MYSQL_ROW row = mysql_fetch_row(db_res->n);
    const unsigned long *const l = mysql_fetch_lengths(db_res->n);
    assert(l != NULL);

    assert((0 == l[0]) == (NULL == row[0]));
    *out_db = std::string(row[0], l[0]);
    if (out_db->size() == 0) {
        return true;
    }

    return true;
}

bool
lowLevelSetCurrentDatabase(const std::unique_ptr<Connect> &c,
                           const std::string &db)
{
    // Use HACK to get this connection to use NULL as default DB.
    if (db.size() == 0) {
        const std::string random_name = getpRandomName();
        RFIF(c->execute("CREATE DATABASE " + random_name + ";"));
        RFIF(c->execute("USE " + random_name + ";"));
        RFIF(c->execute("DROP DATABASE " + random_name + ";"));

        return true;
    }

    const std::string query = "USE " + quoteText(db) + ";";
    RFIF(c->execute(query));

    return true;
}

static void
dropAll(const std::unique_ptr<Connect> &conn)
{
    for (const udf_func * const u: udf_list) {
        const std::string s =
            "DROP FUNCTION IF EXISTS " + convert_lex_str(u->name) + ";";
        assert_s(conn->execute(s), s);
    }
}

std::vector<std::string>
getAllUDFs()
{
    std::vector<std::string> udfs;
    for (const udf_func * const u: udf_list) {
        std::stringstream ss;
        ss << "CREATE ";
        if (u->type == UDFTYPE_AGGREGATE) ss << "AGGREGATE ";
        ss << "FUNCTION " << u->name.str << " RETURNS ";
        switch (u->returns) {
            case INT_RESULT:    ss << "INTEGER"; break;
            case STRING_RESULT: ss << "STRING";  break;
            default:            thrower() << "unknown return " << u->returns;
        }
        ss << " SONAME 'edb.so';";
        udfs.push_back(ss.str());
    }

    return udfs;
}

static void
createAll(const std::unique_ptr<Connect> &conn)
{
    auto udfs = getAllUDFs();
    for (auto it : udfs) {
        assert_s(conn->execute(it), it);
    }
}

//建立cryptdb_udf,保持当前db,删除原有的函数,create一系列的function.回复currentdb.
static void
loadUDFs(const std::unique_ptr<Connect> &conn) {
    const std::string udf_db = "cryptdb_udf";
    assert_s(conn->execute("DROP DATABASE IF EXISTS " + udf_db), "cannot drop db for udfs even with 'if exists'");
    assert_s(conn->execute("CREATE DATABASE " + udf_db), "cannot create db for udfs");

    std::string saved_db;
    assert(lowLevelGetCurrentDatabase(conn, &saved_db));
    assert(lowLevelSetCurrentDatabase(conn, udf_db));
    dropAll(conn);
    createAll(conn);
    assert(lowLevelSetCurrentDatabase(conn, saved_db));

    LOG(cdb_v) << "Loaded CryptDB's UDFs.";
}

static bool
synchronizeDatabases(const std::unique_ptr<Connect> &conn,
                     const std::unique_ptr<Connect> &e_conn)
{
    std::string current_db;
    RFIF(lowLevelGetCurrentDatabase(conn, &current_db));
    RFIF(lowLevelSetCurrentDatabase(e_conn, current_db));

    return true;
}

SharedProxyState::SharedProxyState(ConnectionInfo ci,
                                   const std::string &embed_dir,
                                   const std::string &master_key,
                                   SECURITY_RATING default_sec_rating)
    : masterKey(std::unique_ptr<AES_KEY>(getKey(master_key))),
      embed_dir(embed_dir),
      mysql_dummy(SharedProxyState::db_init(embed_dir)), // HACK: Allows
                                                   // connections in init
                                                   // list.
      conn(new Connect(ci.server, ci.user, ci.passwd, ci.port)),
      default_sec_rating(default_sec_rating),
      cache(std::move(SchemaCache()))
{

    // make sure the server was not started in SQL_SAFE_UPDATES mode
    // > it might not even be possible to start the server in this mode;
    //   better to be safe
    {
        std::unique_ptr<DBResult> dbres;
        assert(conn->execute("SELECT @@sql_safe_updates", &dbres));
        assert(1 == mysql_num_rows(dbres->n));

        const MYSQL_ROW row = mysql_fetch_row(dbres->n);
        const unsigned long *const l = mysql_fetch_lengths(dbres->n);
        const unsigned long value = std::stoul(std::string(row[0], l[0]));
        assert(0 == value);
    }

    std::unique_ptr<Connect>
        init_e_conn(Connect::getEmbedded(embed_dir));
    assert(conn && init_e_conn);

    const std::string prefix = 
        getenv("CRYPTDB_NAME") ? getenv("CRYPTDB_NAME")
                               : "generic_prefix_";
    //初始化embedded的表
    assert(MetaData::initialize(conn, init_e_conn, prefix));
    //两头的defaultdb是一样的
    TEST_TextMessageError(synchronizeDatabases(conn, init_e_conn),
                          "Failed to synchronize embedded and remote"
                          " databases!");

    loadUDFs(conn);

    assert(loadStoredProcedures(conn));
}

SharedProxyState::~SharedProxyState() {

}

int
SharedProxyState::db_init(const std::string &embed_dir)
{
    init_mysql(embed_dir);
    return 1;
}

ProxyState::~ProxyState() {}

SECURITY_RATING
ProxyState::defaultSecurityRating() const
{
    return shared.defaultSecurityRating();
}

const std::unique_ptr<AES_KEY> &
ProxyState::getMasterKey() const
{
    return shared.getMasterKey();
}

const std::unique_ptr<Connect> &
ProxyState::getConn() const
{
    return shared.getConn();
}

const std::unique_ptr<Connect> &
ProxyState::getEConn() const {
    return e_conn;
}

static void
embeddedTHDCleanup(THD *thd) {
    thd->clear_data_list();
    --thread_count;
    // thd->unlink() is called in by THD destructor
    // > THD::~THD()
    //     ilink::~ilink()
    //       ilink::unlink()
    // free_root(thd->main_mem_root, 0) is called in THD::~THD
    delete thd;
}

/*???*/
void
ProxyState::safeCreateEmbeddedTHD() {
    //THD is created by new, so there is no Lex or other things in it.    
    THD *thd = static_cast<THD *>(create_embedded_thd(0));
    assert(thd);
    thds.push_back(std::unique_ptr<THD,
                                   void (*)(THD *)>(thd,
                                       &embeddedTHDCleanup));
    return;
}

void ProxyState::dumpTHDs(){
    for (auto &it : thds) {
        it.release();
    }
    thds.clear();

    assert(0 == thds.size());
}

std::string Delta::tableNameFromType(TableType table_type) const {
    switch (table_type) {
        case REGULAR_TABLE: {
            return MetaData::Table::metaObject();
        }
        case BLEEDING_TABLE: {
            return MetaData::Table::bleedingMetaObject();
        }
        default: {
            FAIL_TextMessageError("Unrecognized table type!");
        }
    }
}


/*insert into the metadata table (kv) and then apply this to childrens*/
static 
bool create_delta_helper(CreateDelta* this_is, const std::unique_ptr<Connect> &e_conn,
                         Delta::TableType table_type, std::string table_name, 
                         const DBMeta &meta_me, const DBMeta &parent,
                         const AbstractMetaKey &meta_me_key, const unsigned int parent_id){
        /*serialize the metame and meta_me_key, and escape*/
        const std::string &child_serial = meta_me.serialize(parent);
        assert(0 == meta_me.getDatabaseID());
        const std::string &serial_key = meta_me_key.getSerial();
        const std::string &esc_serial_key =
            escapeString(e_conn, serial_key);
        const std::string &esc_child_serial =
            escapeString(e_conn, child_serial);

        /*id is 0 for the first time, and after that we can fetch the id from the cache*/
        AssignOnce<unsigned int> old_object_id;
        if (Delta::BLEEDING_TABLE == table_type){
            old_object_id = 0;
        } else {
            assert(Delta::REGULAR_TABLE == table_type);
            auto const &cached = this_is->get_id_cache().find(&meta_me);
            assert(cached != this_is->get_id_cache().end());
            old_object_id = cached->second;
        }
        /*(serial_object, serial_key, parent_id, id) is (meta_me,meta_me_key,parent_id,0)*/
        const std::string &query =
            " INSERT INTO " + table_name + 
            "    (serial_object, serial_key, parent_id, id) VALUES (" 
            " '" + esc_child_serial + "',"
            " '" + esc_serial_key + "',"
            " " + std::to_string(parent_id) + ","
            " " + std::to_string(old_object_id.get()) + ");";

        RETURN_FALSE_IF_FALSE(e_conn->execute(query));

        //this is the id of meta_me, which should be the parent_id for the next layer.
        const unsigned int object_id = e_conn->last_insert_id();

        /*we first insert into bleeding_table {meta_me,last_insert_id}*/
        if (Delta::BLEEDING_TABLE == table_type) {
            assert(this_is->get_id_cache().find(&meta_me) == this_is->get_id_cache().end());
            this_is->get_id_cache()[&meta_me] = object_id;
        } else {
            /*and then erase the item from cache*/
            assert(Delta::REGULAR_TABLE == table_type);
            // should only be used one time
            this_is->get_id_cache().erase(&meta_me);
        }
        std::function<bool(const DBMeta &)> localCreateHandler =
            [&meta_me, object_id, this_is,&e_conn,table_type,table_name]
                (const DBMeta &child){
                return create_delta_helper(this_is,e_conn, table_type, table_name,
                             child, meta_me, meta_me.getKey(child), object_id);
            };
        return meta_me.applyToChildren(localCreateHandler);
}

// Recursive.
// > the hackery around BLEEDING v REGULAR ensures that both tables use the
//   same ID for equivalent objects regardless of differences between
//   auto_increment on the BLEEDING and REGULAR tables
bool CreateDelta::apply(const std::unique_ptr<Connect> &e_conn,
                        Delta::TableType table_type){
    //第一次apply,先写bleeding table.这个时候,map里面没有内容.
    if (BLEEDING_TABLE == table_type) {
        assert(0 == id_cache.size());
    }
    const std::string &table_name = tableNameFromType(table_type);
    const bool b =
        create_delta_helper(this,e_conn,table_type,table_name,
                  *meta.get(), parent_meta, key, parent_meta.getDatabaseID());
    if (BLEEDING_TABLE == table_type) {
        assert(0 != this->id_cache.size());
    } else {
        assert(REGULAR_TABLE == table_type);
        assert(0 == this->id_cache.size());
    }
    return b;
}


// FIXME: used incorrectly, as we should be doing copy construction
// on the original object; not modifying it in place
bool ReplaceDelta::apply(const std::unique_ptr<Connect> &e_conn,
                         TableType table_type)
{
    const std::string table_name = tableNameFromType(table_type);

    const unsigned int child_id = meta.getDatabaseID();

    const std::string child_serial = meta.serialize(parent_meta);
    const std::string esc_child_serial =
        escapeString(e_conn, child_serial);
    const std::string serial_key = key.getSerial();
    const std::string esc_serial_key = escapeString(e_conn, serial_key);

    const std::string query =
        " UPDATE " + table_name +
        "    SET serial_object = '" + esc_child_serial + "', "
        "        serial_key = '" + esc_serial_key + "'"
        "  WHERE id = " + std::to_string(child_id) + ";";
    RETURN_FALSE_IF_FALSE(e_conn->execute(query));

    return true;
}

bool DeleteDelta::apply(const std::unique_ptr<Connect> &e_conn,
                        TableType table_type){
    const std::string table_name = tableNameFromType(table_type);
    Connect * const e_c = e_conn.get();
    std::function<bool(const DBMeta &, const DBMeta &)> helper =
        [&e_c, &helper, table_name](const DBMeta &object,
                                    const DBMeta &parent)
    {
        const unsigned int object_id = object.getDatabaseID();
        const unsigned int parent_id = parent.getDatabaseID();

        const std::string query =
            " DELETE " + table_name + " "
            "   FROM " + table_name +
            "  WHERE " + table_name + ".id" +
            "      = "     + std::to_string(object_id) +
            "    AND " + table_name + ".parent_id" +
            "      = "     + std::to_string(parent_id) + ";";
        RETURN_FALSE_IF_FALSE(e_c->execute(query));

        std::function<bool(const DBMeta &)> localDestroyHandler =
            [&object, &helper] (const DBMeta &child) {
                return helper(child, object);
            };
        return object.applyToChildren(localDestroyHandler);
    };

    return helper(meta, parent_meta);
}

bool
writeDeltas(const std::unique_ptr<Connect> &e_conn,
            const std::vector<std::unique_ptr<Delta> > &deltas,
            Delta::TableType table_type)
{
    for (const auto &it : deltas) {
        RFIF(it->apply(e_conn, table_type));
    }
    return true;
}

bool
deltaOutputBeforeQuery(const std::unique_ptr<Connect> &e_conn,
                       const std::string &original_query,
                       const std::string &rewritten_query,
                       const std::vector<std::unique_ptr<Delta> > &deltas,
                       CompletionType completion_type,
                       uint64_t *const embedded_completion_id)
{

    const std::string &escaped_original_query =
        escapeString(e_conn, original_query);
    const std::string &escaped_rewritten_query =
        escapeString(e_conn, rewritten_query);



    RFIF(escaped_original_query.length()  <= STORED_QUERY_LENGTH
      && escaped_rewritten_query.length() <= STORED_QUERY_LENGTH);

    RFIF(e_conn->execute("START TRANSACTION;"));

    // We must save the current default database because recovery
    // may be happening after a restart in which case such state
    // was lost.
    // FIXME: NOTE: was previously escaping against remote database
    const std::string &q_completion =
        " INSERT INTO " + MetaData::Table::embeddedQueryCompletion() +
        "   (complete, original_query, rewritten_query, default_db, aborted, type)"
        "   VALUES (FALSE, '" + escaped_original_query + "',"
        "          '" + escaped_rewritten_query + "',"
        "           (SELECT DATABASE()),  FALSE,"
        "           '" + TypeText<CompletionType>::toText(completion_type) + "'"
        "          );";
    ROLLBACK_AND_RFIF(e_conn->execute(q_completion), e_conn);
    *embedded_completion_id = e_conn->last_insert_id();
    assert(*embedded_completion_id);

    ROLLBACK_AND_RFIF(writeDeltas(e_conn, deltas, Delta::BLEEDING_TABLE), e_conn);

    ROLLBACK_AND_RFIF(e_conn->execute("COMMIT;"), e_conn);

    return true;
}

bool
deltaOutputAfterQuery(const std::unique_ptr<Connect> &e_conn,
                      const std::vector<std::unique_ptr<Delta> > &deltas,
                      uint64_t embedded_completion_id)
{

    RFIF(e_conn->execute("START TRANSACTION;"));

    const std::string q_update =
        " UPDATE " + MetaData::Table::embeddedQueryCompletion() +
        "    SET complete = TRUE"
        "  WHERE id=" +
                 std::to_string(embedded_completion_id) + ";";
    ROLLBACK_AND_RFIF(e_conn->execute(q_update), e_conn);

    ROLLBACK_AND_RFIF(writeDeltas(e_conn, deltas, Delta::REGULAR_TABLE), e_conn);

    ROLLBACK_AND_RFIF(e_conn->execute("COMMIT;"), e_conn);

    return true;
}

static bool
tableCopy(const std::unique_ptr<Connect> &c, const std::string &src,
          const std::string &dest)
{

    const std::string delete_query =
        " DELETE FROM " + dest + ";";
    RETURN_FALSE_IF_FALSE(c->execute(delete_query));

    const std::string insert_query =
        " INSERT " + dest +
        "   SELECT * FROM " + src + ";";
    RETURN_FALSE_IF_FALSE(c->execute(insert_query));

    return true;
}

bool
setRegularTableToBleedingTable(const std::unique_ptr<Connect> &e_conn)
{

    const std::string src = MetaData::Table::bleedingMetaObject();
    const std::string dest = MetaData::Table::metaObject();
    return tableCopy(e_conn, src, dest);
}

bool
setBleedingTableToRegularTable(const std::unique_ptr<Connect> &e_conn)
{

    const std::string src = MetaData::Table::metaObject();
    const std::string dest = MetaData::Table::bleedingMetaObject();
    return tableCopy(e_conn, src, dest);
}

bool Analysis::addAlias(const std::string &alias,
                        const std::string &db,
                        const std::string &table)
{

    auto db_alias_pair = table_aliases.find(db);
    if (table_aliases.end() == db_alias_pair) {
        table_aliases.insert(
           make_pair(db,
                     std::map<const std::string, const std::string>()));
    }

    std::map<const std::string, const std::string> &
        per_db_table_aliases = table_aliases[db];
    auto alias_pair = per_db_table_aliases.find(alias);
    if (per_db_table_aliases.end() != alias_pair) {
        return false;
    }

    per_db_table_aliases.insert(make_pair(alias, table));
    return true;
}

OnionMeta &Analysis::getOnionMeta(const std::string &db,
                                  const std::string &table,
                                  const std::string &field,
                                  onion o) const
{
    return this->getOnionMeta(this->getFieldMeta(db, table, field), o);
}

OnionMeta &Analysis::getOnionMeta(const FieldMeta &fm,
                                  onion o) const
{
    OnionMeta *const om = fm.getOnionMeta(o);
    //TEST_IdentifierNotFound(om, TypeText<onion>::toText(o));    
    return *om;
}

OnionMeta *Analysis::getOnionMeta2(const FieldMeta &fm,
                                  onion o) const
{
    OnionMeta *const om = fm.getOnionMeta(o);
    //TEST_IdentifierNotFound(om, TypeText<onion>::toText(o));    
    return om;
}


OnionMeta *Analysis::getOnionMeta2(const std::string &db,
                                  const std::string &table,
                                  const std::string &field,
                                  onion o) const
{
    return this->getOnionMeta2(this->getFieldMeta(db, table, field), o);
}


FieldMeta &Analysis::getFieldMeta(const std::string &db,
                                  const std::string &table,
                                  const std::string &field) const
{
    FieldMeta * const fm =
        this->getTableMeta(db, table).getChild(IdentityMetaKey(field));
    TEST_IdentifierNotFound(fm, field);
    return *fm;
}

FieldMeta &Analysis::getFieldMeta(const TableMeta &tm,
                                  const std::string &field) const
{
    FieldMeta *const fm = tm.getChild(IdentityMetaKey(field));
    TEST_IdentifierNotFound(fm, field);

    return *fm;
}

TableMeta &Analysis::getTableMeta(const std::string &db,
                                  const std::string &table) const
{
    const DatabaseMeta &dm = this->getDatabaseMeta(db);

    TableMeta *const tm =
        dm.getChild(IdentityMetaKey(unAliasTable(db, table)));
    TEST_IdentifierNotFound(tm, table);

    return *tm;
}

DatabaseMeta &
Analysis::getDatabaseMeta(const std::string &db) const
{
    DatabaseMeta *const dm = this->schema.getChild(IdentityMetaKey(db));
    TEST_DatabaseNotFound(dm, db);

    return *dm;
}

bool Analysis::tableMetaExists(const std::string &db,
                               const std::string &table) const
{
    return this->nonAliasTableMetaExists(db, unAliasTable(db, table));
}

bool Analysis::nonAliasTableMetaExists(const std::string &db,
                                       const std::string &table) const
{
    const DatabaseMeta &dm = this->getDatabaseMeta(db);
    return dm.childExists(IdentityMetaKey(table));
}

//这里的schema是一个层次的结构,通过analysis中的schema来判断database是不是存在的. 
bool
Analysis::databaseMetaExists(const std::string &db) const
{
    return this->schema.childExists(IdentityMetaKey(db));
}

std::string Analysis::getAnonTableName(const std::string &db,
                                       const std::string &table,
                                       bool *const is_alias) const
{
    // tell the caller if you are giving him an alias
    if (is_alias) {
        *is_alias = this->isAlias(db, table);
    }

    if (this->isAlias(db, table)) {
        return table;
    }

    return this->getTableMeta(db, table).getAnonTableName();
}

std::string
Analysis::translateNonAliasPlainToAnonTableName(const std::string &db,
                                                const std::string &table)
    const
{
    TableMeta *const tm =
        this->getDatabaseMeta(db).getChild(IdentityMetaKey(table));
    TEST_IdentifierNotFound(tm, table);

    return tm->getAnonTableName();
}

std::string Analysis::getAnonIndexName(const std::string &db,
                                       const std::string &table,
                                       const std::string &index_name,
                                       onion o)
    const
{
    return this->getTableMeta(db, table).getAnonIndexName(index_name, o);
}

std::string Analysis::getAnonIndexName(const TableMeta &tm,
                                       const std::string &index_name,
                                       onion o)
    const
{
    return tm.getAnonIndexName(index_name, o);
}

bool Analysis::isAlias(const std::string &db,
                       const std::string &table) const{
    auto db_alias_pair = table_aliases.find(db);
    if (table_aliases.end() == db_alias_pair) {
        return false;
    }

    return db_alias_pair->second.end() != db_alias_pair->second.find(table);
}

std::string Analysis::unAliasTable(const std::string &db,
                                   const std::string &table) const
{
    auto db_alias_pair = table_aliases.find(db);
    if (table_aliases.end() == db_alias_pair) {
        return table;
    }

    auto alias_pair = db_alias_pair->second.find(table);
    if (db_alias_pair->second.end() == alias_pair) {
        return table;
    }
    
    // We've found an alias!
    return alias_pair->second;
}

const EncLayer &Analysis::getBackEncLayer(const OnionMeta &om)
{
    return *om.getLayers().back().get();
}

SECLEVEL Analysis::getOnionLevel(const OnionMeta &om)
{
    return om.getSecLevel();
}

SECLEVEL Analysis::getOnionLevel(const FieldMeta &fm, onion o)
{
    if (false == fm.hasOnion(o)) {
        return SECLEVEL::INVALID;
    }

    return Analysis::getOnionLevel(this->getOnionMeta(fm, o));
}

const std::vector<std::unique_ptr<EncLayer> > &
Analysis::getEncLayers(const OnionMeta &om)
{
    return om.getLayers();
}

RewritePlanWithAnalysis::RewritePlanWithAnalysis(const EncSet &es_out,
                                                 reason r,
                                            std::unique_ptr<Analysis> a)
    : RewritePlan(es_out, r), a(std::move(a)) {

}

std::string
lexToQuery(const LEX &lex)
{
    std::ostringstream o;
    o << const_cast<LEX &>(lex);
    return o.str();
}


