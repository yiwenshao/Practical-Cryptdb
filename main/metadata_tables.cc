#include <string>
#include <memory>

#include <main/metadata_tables.hh>
#include <main/Connect.hh>
#include <main/macro_util.hh>
//这个文件主要就是本地的表的名字维护以及初始化工作.
std::string
MetaData::Table::metaObject()
{
    return DB::embeddedDB() + "." + Internal::getPrefix() + "MetaObject";
}

std::string
MetaData::Table::bleedingMetaObject()
{
    return DB::embeddedDB() + "." + Internal::getPrefix() +
           "BleedingMetaObject";
}

std::string
MetaData::Table::embeddedQueryCompletion()
{
    return DB::embeddedDB() + "." + Internal::getPrefix() +
           "embeddedQueryCompletion";
}

std::string
MetaData::Table::staleness()
{
    return DB::embeddedDB() + "." + Internal::getPrefix() +
           "staleness";
}

std::string
MetaData::Table::showDirective()
{
    return DB::embeddedDB() + "." + Internal::getPrefix() +
           "showDirective";
}

std::string
MetaData::Table::remoteQueryCompletion()
{
    return DB::remoteDB() + "." + Internal::getPrefix() +
           "remoteQueryCompletion";
}

std::string
MetaData::Proc::activeTransactionP()
{
    return DB::remoteDB() + "." + Internal::getPrefix() + "activeTransactionP";
}

std::string
MetaData::DB::embeddedDB()
{
    static const std::string name = "embedded_db";
    return name;
}

std::string
MetaData::DB::remoteDB()
{
    static const std::string name = "remote_db";
    return name;
}

bool static
hasWhitespace(const std::string &s)
{
    for (auto it : s) {
        if (isspace(it)) {
            return true;
        }
    }

    return false;
}

bool
MetaData::initialize(const std::unique_ptr<Connect> &conn,
                     const std::unique_ptr<Connect> &e_conn,
                     const std::string &prefix)
{
    // HACK: prevents multiple initialization
    static bool initialized = false;
    if (initialized) {
        return false;
        //return true;
    }

    // Prefix handling must be done first.
    if (hasWhitespace(prefix)) {
        return false;
    }
    MetaData::Internal::initPrefix(prefix);

    // Embedded database.
    const std::string create_db =
        " CREATE DATABASE IF NOT EXISTS " + DB::embeddedDB() + ";";
    RETURN_FALSE_IF_FALSE(e_conn->execute(create_db));

    const std::string create_meta_table =
        " CREATE TABLE IF NOT EXISTS " + Table::metaObject() +
        "   (serial_object VARBINARY(500) NOT NULL,"
        "    serial_key VARBINARY(500) NOT NULL,"
        "    parent_id BIGINT NOT NULL,"
        "    id SERIAL PRIMARY KEY)"
        " ENGINE=InnoDB;";
    RETURN_FALSE_IF_FALSE(e_conn->execute(create_meta_table));

    const std::string create_bleeding_table =
        " CREATE TABLE IF NOT EXISTS " + Table::bleedingMetaObject() +
        "   (serial_object VARBINARY(500) NOT NULL,"
        "    serial_key VARBINARY(500) NOT NULL,"
        "    parent_id BIGINT NOT NULL,"
        "    id SERIAL PRIMARY KEY)"
        " ENGINE=InnoDB;";
    RETURN_FALSE_IF_FALSE(e_conn->execute(create_bleeding_table));

    {
        const std::string len(std::to_string(STORED_QUERY_LENGTH));
        const std::string create_embedded_completion =
            " CREATE TABLE IF NOT EXISTS " + Table::embeddedQueryCompletion() +
            "   (complete BOOLEAN NOT NULL,"
            "    original_query VARCHAR(" + len + ") NOT NULL,"
            "    rewritten_query VARCHAR(" + len + ") NOT NULL,"
            "    default_db VARCHAR(500),"      // default database is NULLable
            "    aborted BOOLEAN NOT NULL,"
            "    type VARCHAR(100) NOT NULL,"
            "    id SERIAL PRIMARY KEY)"
            " ENGINE=InnoDB;";
        RETURN_FALSE_IF_FALSE(e_conn->execute(create_embedded_completion));
    }

    const std::string create_staleness =
        " CREATE TABLE IF NOT EXISTS " + Table::staleness() +
        "   (cache_id BIGINT UNIQUE NOT NULL,"
        "    stale BOOLEAN NOT NULL,"
        "    id SERIAL PRIMARY KEY)"
        " ENGINE=InnoDB;";
    RETURN_FALSE_IF_FALSE(e_conn->execute(create_staleness));

    const std::string create_show_directive =
        " CREATE TABLE IF NOT EXISTS " + Table::showDirective() +
        "   (_database VARCHAR(500) NOT NULL,"
        "    _table VARCHAR(500) NOT NULL,"
        "    _field VARCHAR(500) NOT NULL,"
        "    _onion VARCHAR(500) NOT NULL,"
        "    _level VARCHAR(500) NOT NULL,"
        "    id SERIAL PRIMARY KEY)"
        " ENGINE=InnoDB;";
    RETURN_FALSE_IF_FALSE(e_conn->execute(create_show_directive));

    // Remote database.
    const std::string create_remote_db =
        " CREATE DATABASE IF NOT EXISTS " + DB::remoteDB() + ";";
    RETURN_FALSE_IF_FALSE(conn->execute(create_remote_db));

    const std::string create_remote_completion =
        " CREATE TABLE IF NOT EXISTS " + Table::remoteQueryCompletion() +
        "   (embedded_completion_id INTEGER NOT NULL,"
        "    completion_type VARCHAR(100) NOT NULL,"
        "    id SERIAL PRIMARY KEY)"
        " ENGINE=InnoDB;";
    RETURN_FALSE_IF_FALSE(conn->execute(create_remote_completion));

    initialized = true;
    return true;
}

void
MetaData::Internal::initPrefix(const std::string &s)
{
    lowLevelPrefix(s.c_str());
}

const std::string &
MetaData::Internal::getPrefix()
{
    return lowLevelPrefix(NULL);
}

const std::string &
MetaData::Internal::lowLevelPrefix(const char *const p)
{
    static const std::string prefix = (assert(p), p);
    return prefix;
}

