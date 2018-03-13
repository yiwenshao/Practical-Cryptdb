#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <list>
#include <algorithm>
#include <stdio.h>
#include <typeinfo>

#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <util/cryptdb_log.hh>
#include <util/enum_text.hh>
#include <util/yield.hpp>
#include <main/CryptoHandlers.hh>
#include <parser/lex_util.hh>
#include <main/sql_handler.hh>
#include <main/dml_handler.hh>
#include <main/ddl_handler.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>

#include "field.h"
#include <errmsg.h>

extern CItemTypesDir itemTypes;
extern CItemFuncDir funcTypes;
extern CItemSumFuncDir sumFuncTypes;
extern CItemFuncNameDir funcNames;

#define ANON                ANON_NAME(__anon_id_)

//TODO: use getAssert in more places
//TODO: replace table/field with FieldMeta * for speed and conciseness

/*
static Item_field *
stringToItemField(const std::string &field,
                  const std::string &table, Item_field *const itf)
{
    THD *const thd = current_thd;
    assert(thd);
    Item_field *const res = new Item_field(thd, itf);
    res->name = NULL; //no alias
    res->field_name = make_thd_string(field);
    res->table_name = make_thd_string(table);

    return res;
}
*/

std::string global_crash_point = "";

void
crashTest(const std::string &current_point) {
    if (current_point == global_crash_point) {
      throw CrashTestException();
    }
}

static inline std::string
extract_fieldname(Item_field *const i)
{
    std::stringstream fieldtemp;
    fieldtemp << *i;
    return fieldtemp.str();
}

static bool
sanityCheck(FieldMeta &fm)
{
    for (const auto &it : fm.getChildren()) {
        OnionMeta *const om = it.second.get();
        const onion o = it.first.getValue();
        const std::vector<SECLEVEL> &secs = fm.getOnionLayout().at(o);
        const auto &layers = om->getLayers();
        for (size_t i = 0; i < layers.size(); ++i) {
            const auto &layer = layers[i];
            assert(layer->level() == secs[i]);
        }
    }
    return true;
}

static bool
sanityCheck(TableMeta &tm)
{
    for (const auto &it : tm.getChildren()) {
        const auto &fm = it.second;
        assert(sanityCheck(*fm.get()));
    }
    return true;
}

static bool
sanityCheck(DatabaseMeta &dm)
{
    for (const auto &it : dm.getChildren()) {
        const auto &tm = it.second;
        assert(sanityCheck(*tm.get()));
    }
    return true;
}

static bool
sanityCheck(SchemaInfo &schema)
{
    for (const auto &it : schema.getChildren()) {
        const auto &dm = it.second;
        assert(sanityCheck(*dm.get()));
    }
    return true;
}

static std::map<std::string, int>
collectTableNames(const std::string &db_name,
                  const std::unique_ptr<Connect> &c)
{
    std::map<std::string, int> name_map;

    assert(c->execute("USE " + quoteText(db_name)));

    std::unique_ptr<DBResult> dbres;
    assert(c->execute("SHOW TABLES", &dbres));

    assert(1 == mysql_num_fields(dbres->n));
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(dbres->n))) {
        const unsigned long *const l = mysql_fetch_lengths(dbres->n);
        const std::string table_name(row[0], l[0]);
        // all table names should be unique
        assert(name_map.end() == name_map.find(table_name));
        name_map[table_name] = 1;
    }
    assert(mysql_num_rows(dbres->n) == name_map.size());

    return name_map;
}

// compares SHOW TABLES on embedded and remote with the SchemaInfo
static bool
tablesSanityCheck(SchemaInfo &schema,
                  const std::unique_ptr<Connect> &e_conn,
                  const std::unique_ptr<Connect> &conn)
{
    for (const auto &dm_it : schema.getChildren()) {
        const auto &db_name = dm_it.first.getValue();
        const auto &dm = dm_it.second;
        // gather anonymous tables
        std::map<std::string, int> anon_name_map =
            collectTableNames(db_name, conn);

        // gather plain tables
        std::map<std::string, int> plain_name_map =
            collectTableNames(db_name, e_conn);

        const auto &meta_tables = dm->getChildren();
        assert(meta_tables.size() == anon_name_map.size());
        assert(meta_tables.size() == plain_name_map.size());
        for (const auto &tm_it : meta_tables) {
            const auto &tm = tm_it.second;

            assert(anon_name_map.find(tm->getAnonTableName())
                   != anon_name_map.end());
            anon_name_map.erase(tm->getAnonTableName());

            assert(plain_name_map.find(tm_it.first.getValue())
                   != plain_name_map.end());
            plain_name_map.erase(tm_it.first.getValue());
        }
        //why earse here?
        assert(0 == anon_name_map.size());
        assert(0 == plain_name_map.size());
    }

    return true;
}

struct RecoveryDetails {
    const bool embedded_complete;
    const bool remote_complete;
    const std::string original_query;
    const std::string rewritten_query;
    const std::string default_db;

    RecoveryDetails(bool embedded_complete, bool remote_complete,
                    const std::string &original_query,
                    const std::string &rewritten_query,
                    const std::string &default_db)
        : embedded_complete(embedded_complete),
          remote_complete(remote_complete), original_query(original_query),
          rewritten_query(rewritten_query), default_db(default_db) {}
};

static bool
collectRecoveryDetails(const std::unique_ptr<Connect> &conn,
                       const std::unique_ptr<Connect> &e_conn,
                       unsigned long unfinished_id,
                       std::unique_ptr<RecoveryDetails> *details)
{
    // collect completion data
    std::unique_ptr<DBResult> dbres;
    const std::string &embedded_completion_q =
        " SELECT complete, original_query, rewritten_query, default_db FROM " +
            MetaData::Table::embeddedQueryCompletion() +
        "  WHERE id = " + std::to_string(unfinished_id) + ";";
    RETURN_FALSE_IF_FALSE(e_conn->execute(embedded_completion_q, &dbres));
    assert(mysql_num_rows(dbres->n) == 1);

    const MYSQL_ROW embedded_row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    const std::string string_embedded_complete(  embedded_row[0],   l[0]);
    const std::string original_query(            embedded_row[1],   l[1]);
    const std::string rewritten_query(           embedded_row[2],   l[2]);
    const std::string default_db(                embedded_row[3],   l[3]);

    const std::string &remote_completion_q =
        " SELECT COUNT(*) FROM " + MetaData::Table::remoteQueryCompletion() +
        "  WHERE embedded_completion_id = " +
                 std::to_string(unfinished_id) + ";";
    RETURN_FALSE_IF_FALSE(conn->execute(remote_completion_q, &dbres));

    assert(1 == mysql_num_rows(dbres->n));
    const MYSQL_ROW remote_row = mysql_fetch_row(dbres->n);
    const unsigned long *const l_remote = mysql_fetch_lengths(dbres->n);

    const long long remote_count =
        std::stoll(std::string(remote_row[0], l_remote[0]));
    assert(remote_count <= 1);
    const bool remote_complete = remote_count == 1;

    const bool embedded_complete = string_to_bool(string_embedded_complete);
    if (embedded_complete) {
        assert(remote_complete);
    }

    *details =
        std::unique_ptr<RecoveryDetails>(
            new RecoveryDetails(embedded_complete, remote_complete,
                                original_query, rewritten_query,
                                default_db));

    return true;
}

static bool
abortQuery(const std::unique_ptr<Connect> &e_conn,
           unsigned long unfinished_id)
{
    const std::string update_aborted =
        " UPDATE " + MetaData::Table::embeddedQueryCompletion() +
        "    SET aborted = TRUE"
        "  WHERE id = " + std::to_string(unfinished_id) + ";";
    RETURN_FALSE_IF_FALSE(e_conn->execute("START TRANSACTION"));
    ROLLBACK_AND_RFIF(setBleedingTableToRegularTable(e_conn), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute(update_aborted), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute("COMMIT"), e_conn);

    return true;
}

static bool
finishQuery(const std::unique_ptr<Connect> &e_conn,
            unsigned long unfinished_id)
{
    const std::string update_completed =
        " UPDATE " + MetaData::Table::embeddedQueryCompletion() +
        "    SET complete = TRUE"
        "  WHERE id = " + std::to_string(unfinished_id) + ";";

    RETURN_FALSE_IF_FALSE(e_conn->execute("START TRANSACTION"));
    ROLLBACK_AND_RFIF(setRegularTableToBleedingTable(e_conn), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute(update_completed), e_conn);
    ROLLBACK_AND_RFIF(e_conn->execute("COMMIT"), e_conn);

    return true;
}

// we never issue onion adjustment queries from here
static bool
fixAdjustOnion(const std::unique_ptr<Connect> &conn,
               const std::unique_ptr<Connect> &e_conn,
               unsigned long unfinished_id)
{
    std::unique_ptr<RecoveryDetails> details;
    RETURN_FALSE_IF_FALSE(
        collectRecoveryDetails(conn, e_conn, unfinished_id, &details));
    assert(false == details->embedded_complete);
    assert(""    == details->rewritten_query);

    lowLevelSetCurrentDatabase(e_conn, details->default_db);

    // failure after initial embedded queries and before remote queries
    if (false == details->remote_complete) {
        assert(false == details->embedded_complete);

        return abortQuery(e_conn, unfinished_id);
    }

    assert(true == details->remote_complete);

    // failure after remote queries
    {
        assert(false == details->embedded_complete);

        return finishQuery(e_conn, unfinished_id);
    }
}

/*
    Other interesting error codes
    > ER_DUP_KEY
    > ER_KEY_DOES_NOT_EXIST
*/
static bool
recoverableDeltaError(unsigned int err)
{
    const bool ret =
        ER_DB_CREATE_EXISTS == err ||       // Database already exists.
        ER_TABLE_EXISTS_ERROR == err ||     // Table already exists.
        ER_DUP_FIELDNAME == err ||          // Column already exists.
        ER_DUP_KEYNAME == err ||            // Key already exists.
        ER_DB_DROP_EXISTS == err ||         // Database doesn't exist.
        ER_BAD_TABLE_ERROR == err ||        // Table doesn't exist.
        ER_CANT_DROP_FIELD_OR_KEY == err;   // Key/Col doesn't exist.

    return ret;
}

// we use a blacklist to determine if the query is bad and thus failed at
// the remote server. if the query fails, but not for one of these
// reasons, we can be reasonably sure that it did not succeed initially.
// the blacklist include errors related to 'bad mysq/connection state'.
// if a query fails for connectivity reasons during recovery, we still
// don't know anything about why it failed initially; or even if it
// succeeded initially.
//
// essentially the blacklist is all errors that could be thrown against
// a 'good' query.
//
// blacklist taken from here:
//  http://dev.mysql.com/doc/refman/5.0/en/mysql-stat.html
//
// we could potentially use a whitelist which would contain errors that
// won't be caught by query_parse(...) and will result in the query
// failing to execute remotely.
static bool
queryInitiallyFailedErrors(unsigned int err)
{
    std::map<unsigned int, int> errors{
        {CR_UNKNOWN_ERROR, 1}, {CR_SERVER_GONE_ERROR, 1}, {CR_SERVER_LOST, 1},
        {CR_COMMANDS_OUT_OF_SYNC, 1}, {ER_OUTOFMEMORY, 1}};

    return errors.end() == errors.find(err);
}

enum class QueryStatus {UNKNOWN_ERROR, MALFORMED_QUERY, SUCCESS,
                        RECOVERABLE_ERROR};
static QueryStatus
retryQuery(const std::unique_ptr<Connect> &c, const std::string &query)
{
    if (true == c->execute(query)) {
        return QueryStatus::SUCCESS;
    }

    // the query failed again
    const unsigned int err = c->get_mysql_errno();
    if (true == recoverableDeltaError(err)) {
        // the query possibly succeeded initially and failed immediately
        // afterwards; or the query just failed originally for the same
        // reason
        return QueryStatus::RECOVERABLE_ERROR;
    }

    // the error is not recoverable and we want to determine if the query
    // is failing because it is bad (malformed or otherwise) or because
    // we are having hardware issues (lose of connectivity, etc)
    // > if the query is just _bad_; tell the caller and he can handle
    // gracefully
    // > if there are hardware issues; we will need manual intervention
    if (true == queryInitiallyFailedErrors(err)) {
        return QueryStatus::MALFORMED_QUERY;
    }

    return QueryStatus::UNKNOWN_ERROR;
}

static bool
fixDDL(const std::unique_ptr<Connect> &conn,
       const std::unique_ptr<Connect> &e_conn,
       unsigned long unfinished_id)
{
    std::unique_ptr<RecoveryDetails> details;
    RETURN_FALSE_IF_FALSE(
        collectRecoveryDetails(conn, e_conn, unfinished_id, &details));
    assert(false == details->embedded_complete);

    lowLevelSetCurrentDatabase(e_conn, details->default_db);
    lowLevelSetCurrentDatabase(conn,   details->default_db);

    AssignOnce<QueryStatus> remote_query_status;
    // failure before remote queries complete
    if (false == details->remote_complete) {
        // reissue the rewritten DDL query against the remote database.
        remote_query_status = 
            retryQuery(conn, details->rewritten_query);
        if (QueryStatus::UNKNOWN_ERROR == remote_query_status.get()) {
            assert(false);
            return false;
        }

        // remote is now fully updated
        const std::string &insert_remote_complete =
            " INSERT INTO " + MetaData::Table::remoteQueryCompletion() +
            "  (embedded_completion_id, completion_type) VALUES"
            "  (" + std::to_string(unfinished_id) + ","
            "   '" + TypeText<CompletionType>::toText(CompletionType::DDL) + "'"
            "  );";
        RETURN_FALSE_IF_FALSE(conn->execute(insert_remote_complete));

        if (QueryStatus::MALFORMED_QUERY == remote_query_status.get()) {
            // if the query is bad there is no reason to try it against the
            // embedded database
            return abortQuery(e_conn, unfinished_id);
        }
    } else {
        // query already succeeded initially
        remote_query_status = QueryStatus::SUCCESS;
    }

    switch (remote_query_status.get()) {
    case QueryStatus::SUCCESS:
    case QueryStatus::RECOVERABLE_ERROR:
        break;
    default:
        assert(false);
    }

    // failure after remote queries completed
    {
        assert(false == details->embedded_complete);

        // reissue the original DDL query against the embedded database
        const QueryStatus embedded_query_status =
            retryQuery(e_conn, details->original_query);
        switch (embedded_query_status) {
        // possibly a hardware issue
        case QueryStatus::UNKNOWN_ERROR:
        // a broken query should not have made it this far
        case QueryStatus::MALFORMED_QUERY:
            return false;

        // --------------------------------------
        // cases above this line are 'definitely'
        // an invalid completion
        // --------------------------------------

        // sometimes you can have a valid success after a fail against the
        // remote database; consider the case where the proxy fails immediately
        // after a successfull DDL query (before it can do completion marking)
        case QueryStatus::SUCCESS:
            break;

        case QueryStatus::RECOVERABLE_ERROR:
            // if we originally succeeded, we have to succeed now as well
            if (true == details->remote_complete) {
                return false;
            }
            break;

        default:
            assert(false);
        }

        return finishQuery(e_conn, unfinished_id);
    }
}

static bool
deltaSanityCheck(const std::unique_ptr<Connect> &conn,
                 const std::unique_ptr<Connect> &e_conn)
{
    const std::string embedded_completion =
        MetaData::Table::embeddedQueryCompletion();
    std::unique_ptr<DBResult> dbres;
    
    const std::string unfinished_deltas =
        " SELECT id, type FROM " + embedded_completion +
        "  WHERE complete = FALSE AND aborted != TRUE;";


    RETURN_FALSE_IF_FALSE(e_conn->execute(unfinished_deltas, &dbres));

    const unsigned long long unfinished_count = mysql_num_rows(dbres->n);

    if (!PRETTY_DEMO) {
        std::cerr << GREEN_BEGIN << "there are " << unfinished_count
              << " unfinished deltas" << COLOR_END << std::endl;
    }

    if (0 == unfinished_count) {
        return true;
    } else if (1 < unfinished_count) {
        return false;
    }


    const MYSQL_ROW row = mysql_fetch_row(dbres->n);
    const unsigned long *const l = mysql_fetch_lengths(dbres->n);
    const std::string string_unfinished_id(row[0], l[0]);
    const std::string string_unfinished_type(row[1], l[1]);

    const unsigned long unfinished_id =
        atoi(string_unfinished_id.c_str());
    const CompletionType type =
        TypeText<CompletionType>::toType(string_unfinished_type);

    switch (type) {
        case CompletionType::Onion:
            return fixAdjustOnion(conn, e_conn, unfinished_id);
        case CompletionType::DDL:
            return fixDDL(conn, e_conn, unfinished_id);
        default:
            std::cerr << "unknown completion type" << std::endl;
            return false;
    }
}

// bleeding and regular meta tables must have identical content
// > note that their next auto_increment value will not necessarily be
//   identical
static bool
metaSanityCheck(const std::unique_ptr<Connect> &e_conn)
{
    // same number of elements
    {
        std::unique_ptr<DBResult> regular_dbres;
        assert(e_conn->execute("SELECT * FROM " + MetaData::Table::metaObject(),
                               &regular_dbres));

        std::unique_ptr<DBResult> bleeding_dbres;
        assert(e_conn->execute("SELECT * FROM "
                               + MetaData::Table::bleedingMetaObject(),
                               &bleeding_dbres));

        assert(mysql_num_rows(bleeding_dbres->n)
            == mysql_num_rows(regular_dbres->n));
    }

    // scan through regular
    {
        std::unique_ptr<DBResult> dbres;
        assert(e_conn->execute(
            "SELECT * FROM " + MetaData::Table::metaObject() + " AS m"
            " WHERE NOT EXISTS ("
            "       SELECT * FROM " +
                        MetaData::Table::bleedingMetaObject() + " AS b"
            "       WHERE"
            "           m.serial_object = b.serial_object AND"
            "           m.serial_key    = b.serial_key AND"
            "           m.id            = b.id AND"
            "           m.parent_id     = b.parent_id)",
            &dbres));

        assert(0 == mysql_num_rows(dbres->n));
    }

    // scan through bleeding
    {
        std::unique_ptr<DBResult> dbres;
        assert(e_conn->execute(
            "SELECT * FROM " + MetaData::Table::bleedingMetaObject() + " AS b"
            " WHERE NOT EXISTS ("
            "       SELECT * FROM " +
                        MetaData::Table::metaObject() + " AS m"
            "       WHERE"
            "           m.serial_object = b.serial_object AND"
            "           m.serial_key    = b.serial_key AND"
            "           m.id            = b.id AND"
            "           m.parent_id     = b.parent_id)",
            &dbres));

        assert(0 == mysql_num_rows(dbres->n));
    }

    return true;
}





// This function will not build all of our tables when it is run
// on an empty database.  If you don't have a parent, your table won't be
// built.  We probably want to seperate our database logic into 3 parts.
//  1> Schema buildling (CREATE TABLE IF NOT EXISTS...)
//  2> INSERTing
//  3> SELECTing

/*
*Now I want to separate the original loadSchemaInfo into two parts.
*/


static DBMeta* loadChildren(DBMeta *const parent,const std::unique_ptr<Connect> &e_conn){
    auto kids = parent->fetchChildren(e_conn);
    for (auto it : kids) {
        loadChildren(it,e_conn);
    }
    return parent;
}

std::unique_ptr<SchemaInfo>
loadSchemaInfo(const std::unique_ptr<Connect> &conn,
               const std::unique_ptr<Connect> &e_conn){
    // Must be done before loading the children.
    assert(deltaSanityCheck(conn, e_conn));
    std::unique_ptr<SchemaInfo>schema(new SchemaInfo());
    // Recursively rebuild the AbstractMeta<Whatever> and it's children.
    loadChildren(schema.get(),e_conn);
    //check from the upmost to the lowest
    assert(sanityCheck(*schema.get()));
    //check metaobject and bleeding table are identical
    assert(metaSanityCheck(e_conn));
    // compares SHOW TABLES on embedded and remote with the SchemaInfo
    assert(tablesSanityCheck(*schema.get(), e_conn, conn));
    return std::move(schema);
}

template <typename Type> static void
translatorHelper(std::vector<std::string> texts,
                 std::vector<Type> enums){
    TypeText<Type>::addSet(enums, texts);
}

static bool
buildTypeTextTranslator(){
    // Onions.
    const std::vector<std::string> onion_strings {
        "oINVALID", "oPLAIN", "oEq", "oOrder", "oADD", "oSWP","oASHE"
    };
    const std::vector<onion> onions {
        oINVALID, oPLAIN, oDET, oOPE, oAGG, oSWP,oASHE
    };
    RETURN_FALSE_IF_FALSE(onion_strings.size() == onions.size());
    translatorHelper<onion>(onion_strings, onions);
    // SecLevels.
    const std::vector<std::string> seclevel_strings{
        "RND", "DET", "DETJOIN","OPEFOREIGN" ,"OPE", "HOM", "SEARCH", "PLAINVAL",
        "INVALID","ASHE"
    };
    const std::vector<SECLEVEL> seclevels{
        SECLEVEL::RND, SECLEVEL::DET, SECLEVEL::DETJOIN, SECLEVEL::OPEFOREIGN,SECLEVEL::OPE,
        SECLEVEL::HOM, SECLEVEL::SEARCH, SECLEVEL::PLAINVAL,
        SECLEVEL::INVALID, SECLEVEL::ASHE
    };
    RETURN_FALSE_IF_FALSE(seclevel_strings.size() == seclevels.size());
    translatorHelper(seclevel_strings, seclevels);

    // MYSQL types.
    const std::vector<std::string> mysql_type_strings{
        "MYSQL_TYPE_DECIMAL", "MYSQL_TYPE_TINY", "MYSQL_TYPE_SHORT",
        "MYSQL_TYPE_LONG", "MYSQL_TYPE_FLOAT", "MYSQL_TYPE_DOUBLE",
        "MYSQL_TYPE_NULL", "MYSQL_TYPE_TIMESTAMP", "MYSQL_TYPE_LONGLONG",
        "MYSQL_TYPE_INT24", "MYSQL_TYPE_DATE", "MYSQL_TYPE_TIME",
        "MYSQL_TYPE_DATETIME", "MYSQL_TYPE_YEAR", "MYSQL_TYPE_NEWDATE",
        "MYSQL_TYPE_VARCHAR", "MYSQL_TYPE_BIT", "MYSQL_TYPE_NEWDECIMAL",
        "MYSQL_TYPE_ENUM", "MYSQL_TYPE_SET",
        "MYSQL_TYPE_TINY_BLOB", "MYSQL_TYPE_MEDIUM_BLOB",
        "MYSQL_TYPE_LONG_BLOB", "MYSQL_TYPE_BLOB",
        "MYSQL_TYPE_VAR_STRING", "MYSQL_TYPE_STRING",
        "MYSQL_TYPE_GEOMETRY"
    };
    const std::vector<enum enum_field_types> mysql_types{
        MYSQL_TYPE_DECIMAL, MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT,
        MYSQL_TYPE_LONG, MYSQL_TYPE_FLOAT, MYSQL_TYPE_DOUBLE,
        MYSQL_TYPE_NULL, MYSQL_TYPE_TIMESTAMP, MYSQL_TYPE_LONGLONG,
        MYSQL_TYPE_INT24, MYSQL_TYPE_DATE, MYSQL_TYPE_TIME,
        MYSQL_TYPE_DATETIME, MYSQL_TYPE_YEAR, MYSQL_TYPE_NEWDATE,
        MYSQL_TYPE_VARCHAR, MYSQL_TYPE_BIT,
        MYSQL_TYPE_NEWDECIMAL /* 246 */, MYSQL_TYPE_ENUM /* 247 */,
        MYSQL_TYPE_SET /* 248 */, MYSQL_TYPE_TINY_BLOB /* 249 */,
        MYSQL_TYPE_MEDIUM_BLOB /* 250 */,
        MYSQL_TYPE_LONG_BLOB /* 251 */, MYSQL_TYPE_BLOB /* 252 */,
        MYSQL_TYPE_VAR_STRING /* 253 */, MYSQL_TYPE_STRING /* 254 */,
        MYSQL_TYPE_GEOMETRY /* 255 */
    };
    RETURN_FALSE_IF_FALSE(mysql_type_strings.size() ==
                            mysql_types.size());
    translatorHelper(mysql_type_strings, mysql_types);

    // MYSQL item types.
    const std::vector<std::string> mysql_item_strings{
        "FIELD_ITEM", "FUNC_ITEM", "SUM_FUNC_ITEM", "STRING_ITEM",
        "INT_ITEM", "REAL_ITEM", "NULL_ITEM", "VARBIN_ITEM",
        "COPY_STR_ITEM", "FIELD_AVG_ITEM", "DEFAULT_VALUE_ITEM",
        "PROC_ITEM", "COND_ITEM", "REF_ITEM", "FIELD_STD_ITEM",
        "FIELD_VARIANCE_ITEM", "INSERT_VALUE_ITEM",
        "SUBSELECT_ITEM", "ROW_ITEM", "CACHE_ITEM", "TYPE_HOLDER",
        "PARAM_ITEM", "TRIGGER_FIELD_ITEM", "DECIMAL_ITEM",
        "XPATH_NODESET", "XPATH_NODESET_CMP", "VIEW_FIXER_ITEM"
    };
    const std::vector<enum Item::Type> mysql_item_types{
        Item::Type::FIELD_ITEM, Item::Type::FUNC_ITEM,
        Item::Type::SUM_FUNC_ITEM, Item::Type::STRING_ITEM,
        Item::Type::INT_ITEM, Item::Type::REAL_ITEM,
        Item::Type::NULL_ITEM, Item::Type::VARBIN_ITEM,
        Item::Type::COPY_STR_ITEM, Item::Type::FIELD_AVG_ITEM,
        Item::Type::DEFAULT_VALUE_ITEM, Item::Type::PROC_ITEM,
        Item::Type::COND_ITEM, Item::Type::REF_ITEM,
        Item::Type::FIELD_STD_ITEM, Item::Type::FIELD_VARIANCE_ITEM,
        Item::Type::INSERT_VALUE_ITEM, Item::Type::SUBSELECT_ITEM,
        Item::Type::ROW_ITEM, Item::Type::CACHE_ITEM,
        Item::Type::TYPE_HOLDER, Item::Type::PARAM_ITEM,
        Item::Type::TRIGGER_FIELD_ITEM, Item::Type::DECIMAL_ITEM,
        Item::Type::XPATH_NODESET, Item::Type::XPATH_NODESET_CMP,
        Item::Type::VIEW_FIXER_ITEM
    };
    RETURN_FALSE_IF_FALSE(mysql_item_strings.size() ==
                            mysql_item_types.size());
    translatorHelper(mysql_item_strings, mysql_item_types);

    // ALTER TABLE [table] DISABLE/ENABLE KEYS
    const std::vector<std::string> disable_enable_keys_strings{
        "DISABLE", "ENABLE", "LEAVE_AS_IS"
    };
    const std::vector<enum enum_enable_or_disable>
        disable_enable_keys_types{
        DISABLE, ENABLE, LEAVE_AS_IS
    };
    RETURN_FALSE_IF_FALSE(disable_enable_keys_strings.size() ==
                            disable_enable_keys_types.size());
    translatorHelper(disable_enable_keys_strings,
                     disable_enable_keys_types);

    // Onion Layouts.
    const std::vector<std::string> onion_layout_strings{
        "PLAIN_ONION_LAYOUT", "NUM_ONION_LAYOUT",
        "BEST_EFFORT_NUM_ONION_LAYOUT", "STR_ONION_LAYOUT",
        "BEST_EFFORT_STR_ONION_LAYOUT","CURRENT_NUM_LAYOUT","CURRENT_STR_LAYOUT"
    };
    const std::vector<onionlayout> onion_layouts{
        PLAIN_ONION_LAYOUT, NUM_ONION_LAYOUT,
        BEST_EFFORT_NUM_ONION_LAYOUT, STR_ONION_LAYOUT,
        BEST_EFFORT_STR_ONION_LAYOUT,CURRENT_NUM_LAYOUT,CURRENT_STR_LAYOUT
    };
    RETURN_FALSE_IF_FALSE(onion_layout_strings.size() ==
                            onion_layouts.size());
    translatorHelper(onion_layout_strings, onion_layouts);

    // Geometry type.
    const std::vector<std::string> geometry_type_strings{
        "GEOM_GEOMETRY", "GEOM_POINT", "GEOM_LINESTRING", "GEOM_POLYGON",
        "GEOM_MULTIPOINT", "GEOM_MULTILINESTRING", "GEOM_MULTIPOLYGON",
        "GEOM_GEOMETRYCOLLECTION"
    };
    std::vector<Field::geometry_type> geometry_types{
        Field::GEOM_GEOMETRY, Field::GEOM_POINT, Field::GEOM_LINESTRING,
        Field::GEOM_POLYGON, Field::GEOM_MULTIPOINT,
        Field::GEOM_MULTILINESTRING, Field::GEOM_MULTIPOLYGON,
        Field::GEOM_GEOMETRYCOLLECTION
    };
    RETURN_FALSE_IF_FALSE(geometry_type_strings.size() ==
                            geometry_types.size());
    translatorHelper(geometry_type_strings, geometry_types);

    // Security Rating.
    const std::vector<std::string> security_rating_strings{
        "SENSITIVE", "BEST_EFFORT", "PLAIN"
    };
    const std::vector<SECURITY_RATING> security_rating_types {
        SECURITY_RATING::SENSITIVE, SECURITY_RATING::BEST_EFFORT,
        SECURITY_RATING::PLAIN
    };
    RETURN_FALSE_IF_FALSE(security_rating_strings.size()
                            == security_rating_types.size());
    translatorHelper(security_rating_strings, security_rating_types);

    // Query Completions.
    const std::vector<std::string> completion_strings
    {
        "DDLCompletion", "AdjustOnionCompletion"
    };
    const std::vector<CompletionType> completion_types
    {
        CompletionType::DDL, CompletionType::Onion
    };
    RETURN_FALSE_IF_FALSE(completion_strings.size()
                            == completion_types.size());
    translatorHelper(completion_strings, completion_types);

    return true;
}

// Allows us to preserve boolean return values from
// buildTypeTextTranslator, handle it as a static constant in
// Rewriter and panic when it fails.
static bool
buildTypeTextTranslatorHack(){
    assert(buildTypeTextTranslator());
    return true;
}

//l gets updated to the new level
static std::string
removeOnionLayer(const Analysis &a, const TableMeta &tm,
                 const FieldMeta &fm,
                 OnionMetaAdjustor *const om_adjustor,
                 SECLEVEL *const new_level,
                 std::vector<std::unique_ptr<Delta> > *const deltas)
{
    // Remove the EncLayer.
    EncLayer const &back_el = om_adjustor->popBackEncLayer();

    // Update the Meta.
    deltas->push_back(std::unique_ptr<Delta>(
                        new DeleteDelta(back_el,
                                        om_adjustor->getOnionMeta())));
    const SECLEVEL local_new_level = om_adjustor->getSecLevel();

    //removes onion layer at the DB
    const std::string dbname = a.getDatabaseName();
    const std::string anon_table_name = tm.getAnonTableName();
    Item_field *const salt =
        new Item_field(NULL, dbname.c_str(), anon_table_name.c_str(),
                       fm.getSaltName().c_str());

    const std::string fieldanon = om_adjustor->getAnonOnionName();
    Item_field *const field =
        new Item_field(NULL, dbname.c_str(), anon_table_name.c_str(),
                       fieldanon.c_str());

    Item *const decUDF = back_el.decryptUDF(field, salt);

    std::stringstream query;
    query << " UPDATE " << quoteText(dbname) << "." << anon_table_name
          << "    SET " << fieldanon  << " = " << *decUDF
          << ";";

    std::cerr << GREEN_BEGIN << "\nADJUST: \n" << COLOR_END << terminalEscape(query.str()) << std::endl;

    //execute decryption query

    LOG(cdb_v) << "adjust onions: \n" << query.str() << std::endl;

    *new_level = local_new_level;
    return query.str();
}

/*
 * Adjusts the onion for a field fm/itf to level: tolevel.
 *
 * Issues queries for decryption to the DBMS.
 *
 * Adjusts the schema metadata at the proxy about onion layers. Propagates the
 * changed schema to persistent storage.
 *
 */
std::pair<std::vector<std::unique_ptr<Delta> >,
                 std::list<std::string>>
adjustOnion(const Analysis &a, onion o, const TableMeta &tm,
            const FieldMeta &fm, SECLEVEL tolevel)
{
    TEST_Text(tolevel >= a.getOnionMeta(fm, o).getMinimumSecLevel(),
              "your query requires to permissive of a security level");

    // Make a copy of the onion meta for the purpose of making
    // modifications during removeOnionLayer(...)
    OnionMetaAdjustor om_adjustor(*fm.getOnionMeta(o));
    SECLEVEL newlevel = om_adjustor.getSecLevel();
    assert(newlevel != SECLEVEL::INVALID);

    std::list<std::string> adjust_queries;
    std::vector<std::unique_ptr<Delta> > deltas;
    while (newlevel > tolevel) {
        auto query =
            removeOnionLayer(a, tm, fm, &om_adjustor, &newlevel,
                             &deltas);
        adjust_queries.push_back(query);
    }
    TEST_UnexpectedSecurityLevel(o, tolevel, newlevel);

    return make_pair(std::move(deltas), adjust_queries);
    // return make_pair(deltas, adjust_queries);
}
//TODO: propagate these adjustments in the embedded database?

static inline bool
FieldQualifies(const FieldMeta *const restriction,
               const FieldMeta *const field)
{
    return !restriction || restriction == field;
}

template <class T>
static Item *
do_optimize_const_item(T *i, Analysis &a) {
    return i;

}

//层次化的解密
static Item *
decrypt_item_layers(const Item &i, const FieldMeta *const fm, onion o,
                    uint64_t IV)
{
    assert(!RiboldMYSQL::is_null(i));

    const Item *dec = &i;
    Item *out_i = NULL;
    //有fieldmeta但是不用全部, 只用其中的一个onionMeta, 这个根据OLK的o来选择.
    const OnionMeta *const om = fm->getOnionMeta(o);
    assert(om);
    //onionmeta的使用方法很简单, getlayers, 然后层层使用.
    const auto &enc_layers = om->getLayers();
    for (auto it = enc_layers.rbegin(); it != enc_layers.rend(); ++it) {
        out_i = (*it)->decrypt(*dec, IV);
        assert(out_i);
        dec = out_i;
        LOG(cdb_v) << "dec okay";
    }

    assert(out_i && out_i != &i);
    return out_i;
}


/*
 * Actual item handlers.
 */
static void optimize_select_lex(st_select_lex *select_lex, Analysis & a);

static Item *getLeftExpr(const Item_in_subselect &i)
{
        Item *const left_expr =
        i.*rob<Item_in_subselect, Item*,
                &Item_in_subselect::left_expr>::ptr();
    assert(left_expr);

    return left_expr;

}

// HACK: Forces query down to PLAINVAL.
// if more complicated subqueries begin to give us problems,
// subselect_engine::prepare(...) and Item_subselect::fix_fields(...) may be
// worth investigating
static class ANON : public CItemSubtypeIT<Item_subselect,
                                          Item::Type::SUBSELECT_ITEM> {
    virtual RewritePlan *
    do_gather_type(const Item_subselect &i, Analysis &a) const
    {
        const std::string why = "subselect";

        // create an Analysis object for subquery gathering/rewriting
        std::unique_ptr<Analysis> subquery_analysis(new Analysis(a));
        // aliases should be available to the subquery as well
        subquery_analysis->table_aliases = a.table_aliases;

        // Gather subquery.
        const st_select_lex *const select_lex =
            RiboldMYSQL::get_select_lex(i);
        process_select_lex(*select_lex, *subquery_analysis);

        // HACK: Forces the subquery to use PLAINVAL for it's
        // projections.
        auto item_it =
            RiboldMYSQL::constList_iterator<Item>(select_lex->item_list);
        for (;;) {
            const Item *const item = item_it++;
            if (!item) {
                break;
            }

            const std::unique_ptr<RewritePlan> &item_rp =
                subquery_analysis->rewritePlans[item];
            TEST_NoAvailableEncSet(item_rp->es_out, i.type(),
                                   PLAIN_EncSet, why,
                            std::vector<std::shared_ptr<RewritePlan> >());
            item_rp->es_out = PLAIN_EncSet;
        }

        const EncSet &out_es = PLAIN_EncSet;
        const reason &rsn = reason(out_es, why, i);

        switch (RiboldMYSQL::substype(i)) {
            case Item_subselect::subs_type::SINGLEROW_SUBS:
                break;
            case Item_subselect::subs_type::EXISTS_SUBS:
                break;
            case Item_subselect::subs_type::IN_SUBS: {
                const Item *const left_expr =
                    getLeftExpr(static_cast<const Item_in_subselect &>(i));
                RewritePlan *const rp_left_expr =
                    gather(*left_expr, *subquery_analysis.get());
                a.rewritePlans[left_expr] =
                    std::unique_ptr<RewritePlan>(rp_left_expr);
                break;
            }
            case Item_subselect::subs_type::ALL_SUBS:
                assert(false);
            case Item_subselect::subs_type::ANY_SUBS:
                assert(false);
            default:
                FAIL_TextMessageError("Unknown subquery type!");
        }

        return new RewritePlanWithAnalysis(out_es, rsn,
                                           std::move(subquery_analysis));
    }

    virtual Item * do_optimize_type(Item_subselect *i, Analysis & a) const {
        optimize_select_lex(i->get_select_lex(), a);
        return i;
    }

    virtual Item *
    do_rewrite_type(const Item_subselect &i, const OLK &constr,
                    const RewritePlan &rp, Analysis &a)
        const
    {
        const RewritePlanWithAnalysis &rp_w_analysis =
            static_cast<const RewritePlanWithAnalysis &>(rp);
        const st_select_lex *const select_lex =
            RiboldMYSQL::get_select_lex(i);

        // ------------------------------
        //    General Subquery Rewrite
        // ------------------------------
        st_select_lex *const new_select_lex =
            rewrite_select_lex(*select_lex, *rp_w_analysis.a.get());

        // Rewrite table names.
        new_select_lex->top_join_list =
            rewrite_table_list(select_lex->top_join_list,
                               *rp_w_analysis.a.get());

        /* printing a single row subquery looks like this
         * ...
         * Item_singlerow_subselect::print(...) <--- defers to base class
         *   Item_subselect::print(...)
         *     subselect_engine::print(...)     <--- pure virtual
         *       subselect_single_select_engine::print(...)
         *         st_select_lex::print(...) on the engine ``st_select_lex'' member variable
         *
         * if you can get the engine in the ``Item_subselect'' object to point to
         * our rewritten ``st_select_lex'' you will get the desired results
         *
         * the next step is to properly build a new ``Item_singlerow_subselect'';
         * the constructor for ``Item_singlerow_subselect'' will either create a
         * new engine or use an old one from the ``st_select_lex'' parameter.
         * we want it to use a new one, otherwise it will be the engine from
         * the original Item_subselect.  setting master_unit()->item on our
         * rewritten ``st_select_lex'' to NULL will give us this behavior.
         *
         * the ``Item_singlerow_subselect'' constructor calls
         * Item_subselect::init(...) which takes the ``st_select_lex'' as a
         * parameter. provided the aforementioned NULL condition holds,
         * init(...) then constructs the new ``subselect_single_select_engine''
         * and our rewritten ``Item_singlerow_subselect'' keeps it as a member
         * pointer. The ``subselect_single_select_engine'' constructor then
         * takes the ``st_select_lex'' as a parameter and sets
         * st_select_lex::master_unit()->item as a backpointer to the
         * ``Item_singlerow_subselect'' that owns the engine.
         *
         * sql/item_subselect.{cc,hh} has all the details should you care
         */
        new_select_lex->master_unit()->item = NULL;

        // ------------------------------
        //   Specific Subquery Rewrite
        // ------------------------------
        {
            switch (RiboldMYSQL::substype(i)) {
                case Item_subselect::subs_type::SINGLEROW_SUBS: {
                    Item_singlerow_subselect *const new_item_single =
                        new Item_singlerow_subselect(new_select_lex);
                    // ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
                    //          sanity check
                    // ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
                    // did the old engine get replaced?
                    subselect_single_select_engine *const old_engine =
                        static_cast<subselect_single_select_engine *>(
                                i.*rob<Item_subselect, subselect_engine*,
                                       &Item_subselect::engine>::ptr());
                    subselect_single_select_engine *const rewrite_engine =
                        static_cast<subselect_single_select_engine *>(
                                new_item_single->*rob<Item_subselect, subselect_engine*,
                                                      &Item_subselect::engine>::ptr());
                    assert(old_engine != rewrite_engine);
                    // does the new engine have a backpointer to our
                    // rewritten Item?
                    st_select_lex *const old_select_lex =
                        old_engine->*rob<subselect_single_select_engine,
                                         st_select_lex *,
                                         &subselect_single_select_engine::select_lex>::ptr();
                    st_select_lex *const rewrite_select_lex =
                        rewrite_engine->*rob<subselect_single_select_engine,
                                             st_select_lex *,
                                             &subselect_single_select_engine::select_lex>::ptr();
                    assert(old_select_lex == select_lex);
                    assert(rewrite_select_lex == new_select_lex);
                    assert(rewrite_select_lex->master_unit()->item == new_item_single);

                    return new_item_single;
                }
                case Item_subselect::subs_type::EXISTS_SUBS:
                    return new Item_exists_subselect(new_select_lex);
                case Item_subselect::subs_type::IN_SUBS: {
                    const Item *const left_expr =
                        getLeftExpr(static_cast<const Item_in_subselect &>(i));
                    const std::unique_ptr<RewritePlan> &rp_left_expr =
                        constGetAssert(a.rewritePlans, left_expr);
                    Item *const new_left_expr =
                        itemTypes.do_rewrite(*left_expr, constr,
                                             *rp_left_expr.get(), a);
                    return new Item_in_subselect(new_left_expr,
                                                 new_select_lex);
                }
                case Item_subselect::subs_type::ALL_SUBS:
                    assert(false);
                case Item_subselect::subs_type::ANY_SUBS:
                    assert(false);
                default:
                    FAIL_TextMessageError("Unknown subquery type!");
            }
        }
    }
} ANON;

// NOTE: Shouldn't be needed unless we allow mysql to rewrite subqueries.
static class ANON : public CItemSubtypeIT<Item_cache, Item::Type::CACHE_ITEM> {
    virtual RewritePlan *do_gather_type(const Item_cache &i,
                                        Analysis &a) const
    {
        UNIMPLEMENTED;
        return NULL;

    }

    virtual Item * do_optimize_type(Item_cache *i, Analysis & a) const
    {
        // TODO(stephentu): figure out how to use rob here
        return i;
    }

    virtual Item *do_rewrite_type(const Item_cache &i, const OLK &constr,
                                  const RewritePlan &rp, Analysis &a)
        const
    {
        UNIMPLEMENTED;
        return NULL;
    }
} ANON;

/*
 * Some helper functions.
 */

static void
optimize_select_lex(st_select_lex *select_lex, Analysis & a)
{
    auto item_it = List_iterator<Item>(select_lex->item_list);
    for (;;) {
        if (!item_it++)
            break;
        optimize(item_it.ref(), a);
    }

    if (select_lex->where)
        optimize(&select_lex->where, a);

    if (select_lex->join &&
        select_lex->join->conds &&
        select_lex->where != select_lex->join->conds)
        optimize(&select_lex->join->conds, a);

    if (select_lex->having)
        optimize(&select_lex->having, a);

    for (ORDER *o = select_lex->group_list.first; o; o = o->next)
        optimize(o->item, a);
    for (ORDER *o = select_lex->order_list.first; o; o = o->next)
        optimize(o->item, a);
}

static void
optimize_table_list(List<TABLE_LIST> *tll, Analysis &a)
{
    List_iterator<TABLE_LIST> join_it(*tll);
    for (;;) {
        TABLE_LIST *t = join_it++;
        if (!t)
            break;

        if (t->nested_join) {
            optimize_table_list(&t->nested_join->join_list, a);
            return;
        }

        if (t->on_expr)
            optimize(&t->on_expr, a);

        if (t->derived) {
            st_select_lex_unit *u = t->derived;
            optimize_select_lex(u->first_select(), a);
        }
    }
}

static bool
noRewrite(const LEX &lex) {
    switch (lex.sql_command) {
    case SQLCOM_SHOW_DATABASES:
    // case SQLCOM_SET_OPTION:
    case SQLCOM_BEGIN:
    case SQLCOM_ROLLBACK:
    case SQLCOM_COMMIT:
    case SQLCOM_SHOW_VARIABLES:
    case SQLCOM_UNLOCK_TABLES:
    case SQLCOM_SHOW_STORAGE_ENGINES:
    case SQLCOM_SHOW_COLLATIONS:
        return true;
    case SQLCOM_SELECT: {

    }
    default:
        return false;
    }

    return false;
}

const bool Rewriter::translator_dummy = buildTypeTextTranslatorHack();

const std::unique_ptr<SQLDispatcher> Rewriter::dml_dispatcher =
    std::unique_ptr<SQLDispatcher>(buildDMLDispatcher());

const std::unique_ptr<SQLDispatcher> Rewriter::ddl_dispatcher =
    std::unique_ptr<SQLDispatcher>(buildDDLDispatcher());


static std::string serilize_OnionAdjustExcept(OnionAdjustExcept &e){
    //onion and level
    std::string res;
    if(e.o==oDET){
       res+="oDET";
    }else if(e.o==oOPE){
        res+="oOPE";
    }else if(e.o==oAGG){
        res+="oAGG";
    }else if(e.o==oSWP){
        res+="oSWP";
    }else if(e.o==oPLAIN){
        res+="oPLAIN";
    }else{
        res+="NULLONION";
    }
    res+="::::";
    if(e.tolevel==SECLEVEL::INVALID){
        res+="INVALID";
    }else if(e.tolevel==SECLEVEL::PLAINVAL){
        res+="PLAINVAL";
    }else if(e.tolevel==SECLEVEL::OPEFOREIGN){
        res+="OPEFOREIGN";
    }else if(e.tolevel==SECLEVEL::OPE){
        res+="OPE";
    }else if(e.tolevel==SECLEVEL::DETJOIN){
        res+="DETJOIN";
    }else if(e.tolevel==SECLEVEL::DET){
        res+="DET";
    }else if(e.tolevel==SECLEVEL::SEARCH){
        res+="SEARCH";
    }else if(e.tolevel==SECLEVEL::HOM){
        res+="HOM";
    }else if(e.tolevel==SECLEVEL::RND){
        res+="RND";
    }else{
        res+="nulllevel";
    }
    return res;
}


// NOTE : This will probably choke on multidatabase queries.
/*
*parse the query, rewrite the query using handlers and then return an executor.
*possibly trigger an onion adjustment.
*/
AbstractQueryExecutor *
Rewriter::dispatchOnLex(Analysis &a, const std::string &query)
{
    std::unique_ptr<query_parse> p;
    try {
        p = std::unique_ptr<query_parse>(
                new query_parse(a.getDatabaseName(), query));
    } catch (const CryptDBError &e) {
        FAIL_TextMessageError("Bad Query: [" + query + "]\n"
                              "Error Data: " + e.msg);
    }
    LEX *const lex = p->lex();

    // optimization: do not process queries that we will not rewrite
    if (noRewrite(*lex)) {
        return new SimpleExecutor();
    } else if (dml_dispatcher->canDo(lex)) { 
        // HACK: We don't want to process INFORMATION_SCHEMA queries
        if (SQLCOM_SELECT == lex->sql_command &&
            lex->select_lex.table_list.first) {
            const std::string &db = lex->select_lex.table_list.first->db;
            if (equalsIgnoreCase("INFORMATION_SCHEMA", db)) {
                return new SimpleExecutor();
            }
        }
        const SQLHandler &handler = dml_dispatcher->dispatch(lex);
        AssignOnce<AbstractQueryExecutor *> executor;
        try {
            executor = handler.transformLex(a, lex);
        } catch (OnionAdjustExcept e) {
            LOG(cdb_v) << "caught onion adjustment";

            //We use deltas to remove layers in the metadata, and queyrs to decrypt data.
            std::pair<std::vector<std::unique_ptr<Delta> >,
                      std::list<std::string> >
                out_data = adjustOnion(a, e.o, e.tm, e.fm, e.tolevel);
            std::vector<std::unique_ptr<Delta> > &deltas = out_data.first;
            const std::list<std::string> &adjust_queries = out_data.second;

            return new OnionAdjustmentExecutor(std::move(deltas),
                                               adjust_queries);
        }
        return executor.get();
    } else if (ddl_dispatcher->canDo(lex)) {
        const SQLHandler &handler = ddl_dispatcher->dispatch(lex);
        AbstractQueryExecutor * executor ;
        try{
            executor = handler.transformLex(a, lex);
        }catch(OnionAdjustExcept e){ 
            // if an error occur in the first line of code, gdb will go to return NULL, which is not what exactly happened.
            //We use deltas to remove layers in the metadata, and queyrs to decrypt data.
            std::pair<std::vector<std::unique_ptr<Delta> >,
                      std::list<std::string> >
                out_data = adjustOnion(a, e.o, e.tm, e.fm, e.tolevel);
            
            std::string resadjust = serilize_OnionAdjustExcept(e);
            std::vector<std::unique_ptr<Delta> > &deltas = out_data.first;
            const std::list<std::string> &adjust_queries = out_data.second;
            return new OnionAdjustmentExecutor(std::move(deltas),
                                               adjust_queries);           

        }
        return executor;
    }

    return NULL;
}

QueryRewrite
Rewriter::rewrite(const std::string &q, const SchemaInfo &schema,
                  const std::string &default_db, const ProxyState &ps){
    assert(0 == mysql_thread_init());
    Analysis analysis(default_db, schema, ps.getMasterKey(),
                      ps.defaultSecurityRating());

    AbstractQueryExecutor *const executor =
        Rewriter::dispatchOnLex(analysis, q);
    if (!executor) {
        return QueryRewrite(true, analysis.rmeta, analysis.kill_zone,
                            new NoOpExecutor());       
    }
    return QueryRewrite(true, analysis.rmeta, analysis.kill_zone, executor);
}

//TODO: replace stringify with <<
std::string ReturnField::stringify() {
    std::stringstream res;
    res << " is_salt: " << is_salt << " filed_called " << field_called;
    res << " fm  " << olk.key << " onion " << olk.o;
    res << " salt_pos " << salt_pos;
    return res.str();
}

std::string ReturnMeta::stringify() {
    std::stringstream res;
    res << "rmeta contains " << rfmeta.size() << " elements: \n";
    for (auto it : rfmeta) {
        res << it.first << " " << it.second.stringify() << "\n";
    }
    return res.str();
}


/*Transform encrypted ResType into plaintext ResType
 *ReturnMeta contains metadata form layers of decryption
 *
*/
ResType
Rewriter::decryptResults(const ResType &dbres, const ReturnMeta &rmeta)
{
    assert(dbres.success());

    const unsigned int rows = dbres.rows.size();
    const unsigned int cols = dbres.names.size();

    // un-anonymize the names
    std::vector<std::string> dec_names;

    for (auto it = dbres.names.begin();
        it != dbres.names.end(); it++) {
        const unsigned int index = it - dbres.names.begin();
        //use index to get either salt or metadata for encrypted field.
        const ReturnField &rf = rmeta.rfmeta.at(index);
        if (!rf.getIsSalt()) {
            //plaintext column name
            dec_names.push_back(rf.fieldCalled());
        }
    }

    const unsigned int real_cols = dec_names.size();

    std::vector<std::vector<Item *> > dec_rows(rows);
    for (unsigned int i = 0; i < rows; i++) {
        dec_rows[i] = std::vector<Item *>(real_cols);
    }
    //
    unsigned int col_index = 0;
    for (unsigned int c = 0; c < cols; c++) {
        const ReturnField &rf = rmeta.rfmeta.at(c);
        if (rf.getIsSalt()) {
            continue;
        }
        FieldMeta *const fm = rf.getOLK().key;

        for (unsigned int r = 0; r < rows; r++) {

            if (!fm || dbres.rows[r][c]->is_null()) {
                dec_rows[r][col_index] = dbres.rows[r][c];
            } else {
                uint64_t salt = 0;
                const int salt_pos = rf.getSaltPosition();
                //use salt_pos to read the salt from remote results.
                if (salt_pos >= 0) {
                    Item_int *const salt_item =
                        static_cast<Item_int *>(dbres.rows[r][salt_pos]);
                    assert_s(!salt_item->null_value, "salt item is null");
                    salt = salt_item->value;
                }
                //layers of decryption.
                dec_rows[r][col_index] = 
                    decrypt_item_layers(*dbres.rows[r][c],
                                        fm, rf.getOLK().o, salt);
            }
        }
        col_index++;
    }

    //加密和解密之前之后, 用的都是ResType类型.通过这个解密函数的操作.
    return ResType(dbres.ok, dbres.affected_rows, dbres.insert_id,
                   std::move(dec_names),
                   std::vector<enum_field_types>(dbres.types),
                   std::move(dec_rows));
}


void
printRes(const ResType &r) {
    std::stringstream ssn;
    for (unsigned int i = 0; i < r.names.size(); i++) {
        char buf[400];
        snprintf(buf, sizeof(buf), "%-25s", r.names[i].c_str());
        ssn << buf;
    }
    std::cerr << terminalEscape(ssn.str()) << std::endl;
    //LOG(edb_v) << ssn.str();

    /* next, print out the rows */
    for (unsigned int i = 0; i < r.rows.size(); i++) {
        std::stringstream ss;
        for (unsigned int j = 0; j < r.rows[i].size(); j++) {
            char buf[400];
            std::stringstream sstr;
            sstr << *r.rows[i][j];
            snprintf(buf, sizeof(buf), "%-25s", sstr.str().c_str());
            ss << buf;
        }
        std::cerr << terminalEscape(ss.str()) << std::endl;
    }
}

EncLayer &OnionMetaAdjustor::getBackEncLayer() const
{
    return *duped_layers.back();
}

EncLayer &OnionMetaAdjustor::popBackEncLayer()
{
    EncLayer &out_layer = *duped_layers.back();
    duped_layers.pop_back();

    return out_layer;
}

SECLEVEL OnionMetaAdjustor::getSecLevel() const
{
    
    return duped_layers.back()->level();
}

const OnionMeta &OnionMetaAdjustor::getOnionMeta() const
{
    return original_om;
}

std::string OnionMetaAdjustor::getAnonOnionName() const
{
    return original_om.getAnonOnionName();
}

std::vector<EncLayer *>
OnionMetaAdjustor::pullCopyLayers(OnionMeta const &om)
{
    std::vector<EncLayer *> v;

    auto const &enc_layers = om.getLayers();
    for (const auto &it : enc_layers) {
        v.push_back(it.get());
    }

    return v;
}


//Write delta into the local meta database to remove layers, issuses queries to adjust layers, and then reissus the original query.
std::pair<AbstractQueryExecutor::ResultType, AbstractAnything *>
OnionAdjustmentExecutor::
nextImpl(const ResType &res, const NextParams &nparams)
{
    reenter(this->corot) {
        yield {
            assert(this->adjust_queries.size() == 1
                   || this->adjust_queries.size() == 2);

            {
                uint64_t embedded_completion_id;
                deltaOutputBeforeQuery(nparams.ps.getEConn(),
                                       nparams.original_query, "",
                                       this->deltas,
                                       CompletionType::Onion,
                                       &embedded_completion_id);
                this->embedded_completion_id = embedded_completion_id;
            }

            return CR_QUERY_AGAIN(
                "CALL " + MetaData::Proc::activeTransactionP());
        }
        TEST_ErrPkt(res.success(),
                    "failed to determine if there is an active transasction");
        this->in_trx = handleActiveTransactionPResults(res);

        // always rollback
        yield return CR_QUERY_AGAIN("ROLLBACK");
        TEST_ErrPkt(res.success(), "failed to rollback");

        yield return CR_QUERY_AGAIN("START TRANSACTION");
        TEST_ErrPkt(res.success(), "failed to start transaction");

        // issue first adjustment
        yield return CR_QUERY_AGAIN(this->adjust_queries.front());
        CR_ROLLBACK_AND_FAIL(res,
                        "failed to execute first onion adjustment query!");

        // issue (possible) second adjustment
        yield {
            assert(res.success());

            return CR_QUERY_AGAIN(
                    this->adjust_queries.size() == 2 ? this->adjust_queries.back()
                                                     : "DO 0;");
        }
        CR_ROLLBACK_AND_FAIL(res,
                        "failed to execute second onion adjustment query!");

        yield {
            return CR_QUERY_AGAIN(
                " INSERT INTO " + MetaData::Table::remoteQueryCompletion() +
                "   (embedded_completion_id, completion_type) VALUES"
                "   (" + std::to_string(this->embedded_completion_id.get()) + ","
                "   '"+TypeText<CompletionType>::toText(CompletionType::Onion)+"'"
                "        );");
        }
        TEST_ErrPkt(res.success(), "failed issuing adjustment completion");

        yield return CR_QUERY_AGAIN("COMMIT");
        TEST_ErrPkt(res.success(), "failed to commit");

        TEST_ErrPkt(deltaOutputAfterQuery(nparams.ps.getEConn(), this->deltas,
                                          this->embedded_completion_id.get()),
                    "deltaOutputAfterQuery failed for onion adjustment");

        // if the client was in the middle of a transaction we must alert
        // him that we had to rollback his queries
        if (true == this->in_trx.get()) {
            ROLLBACK_ERROR_PACKET
        }

        try {
            this->reissue_query_rewrite = new QueryRewrite(
                Rewriter::rewrite(
                    nparams.original_query, *nparams.ps.getSchemaInfo().get(),
                    nparams.default_db, nparams.ps));
        } catch (const AbstractException &e) {
            FAIL_GenericPacketException(e.to_string());
        } catch (...) {
            FAIL_GenericPacketException(
                "unknown error occured while rewriting onion adjusment query");
        }

        this->reissue_nparams =
            NextParams(nparams.ps, nparams.default_db, nparams.original_query);
        while (true) {
            yield {
                auto result =
                    this->reissue_query_rewrite->executor->next(
                        first_reissue ? ResType(true, 0, 0)
                                      : res,
                        reissue_nparams.get());
                this->first_reissue = false;
                return result;
            }
        }
    }
    assert(false);
}

