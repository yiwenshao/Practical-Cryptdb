#pragma once

#include <util/onions.hh>
#include <util/enum_text.hh>
#include <parser/embedmysql.hh>
#include <parser/stringify.hh>
#include <main/CryptoHandlers.hh>
#include <main/Translator.hh>
#include <main/dbobject.hh>
#include <main/macro_util.hh>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <sstream>
#include <functional>

/*
 * The name must be unique as it is used as a unique identifier when
 * generating the encryption layers.
 *
 * OnionMeta is a bit different than the other AbstractMeta derivations.
 * > It's children aren't of the same class.  Each EncLayer does
 *   inherit from EncLayer, but they are still distinct classes. This
 *   is problematic because DBMeta::deserialize<ConcreteClass> relies
 *   on us being able to provide a concrete class.  We can't pick a
 *   specific class for our OnionMeta as it must support multiple classes.
 * > Also note that like FieldMeta, OnionMeta's children have an explicit
 *   order that must be encoded.
 */
class OnionMeta : public DBMeta {
public:
    OnionMeta(onion o, std::vector<SECLEVEL> levels,
              const AES_KEY * const m_key, const Create_field &cf,
              unsigned long uniq_count, SECLEVEL minimum_seclevel);
    // Restore.
    static std::unique_ptr<OnionMeta>
        deserialize(unsigned int id, const std::string &serial);
    OnionMeta(unsigned int id, const std::string &onionname,
              unsigned long uniq_count, SECLEVEL minimum_seclevel)
        : DBMeta(id), onionname(onionname), uniq_count(uniq_count),
          minimum_seclevel(minimum_seclevel) {}

    std::string serialize(const DBObject &parent) const;
    std::string getAnonOnionName() const;
    TYPENAME("onionMeta")
    std::vector<DBMeta *>
        fetchChildren(const std::unique_ptr<Connect> &e_conn);
    bool applyToChildren(std::function<bool(const DBMeta &)>) const;

    UIntMetaKey const &getKey(const DBMeta &child) const;

    EncLayer *getLayerBack() const;
    EncLayer *getLayer(const SECLEVEL &sl) const;
    bool hasEncLayer(const SECLEVEL &sl) const;
    SECLEVEL getSecLevel() const;

    unsigned long getUniq() const {return uniq_count;}
    const std::vector<std::unique_ptr<EncLayer> > &getLayers() const
        {return layers;}
    SECLEVEL getMinimumSecLevel() const {return minimum_seclevel;}
    void setMinimumSecLevel(SECLEVEL seclevel) {this->minimum_seclevel = seclevel;}

private:
    // first in list is lowest layer
    std::vector<std::unique_ptr<EncLayer> > layers;
    const std::string onionname;
    const unsigned long uniq_count;
    SECLEVEL minimum_seclevel;
    /*what are those keys used for?*/
    mutable std::list<std::unique_ptr<UIntMetaKey>> generated_keys;
};

class UniqueCounter {
public:
    uint64_t leaseCount() {return getCounter_()++;}
    uint64_t currentCount() {return getCounter_();}

private:
    virtual uint64_t &getCounter_() = 0;
};

class FieldMeta : public MappedDBMeta<OnionMeta, OnionMetaKey>,
                  public UniqueCounter {
public:
    // New a fieldmeta, Create_field  and unique are used for determin the characteristics of the new
    //fieldmeta, they are not part of fieldmeta.
    FieldMeta(const Create_field &field, const AES_KEY * const mKey,
              SECURITY_RATING sec_rating, unsigned long uniq_count,
              bool unique);

    // Restore (WARN: Creates an incomplete type as it will not have it's
    // OnionMetas until they are added by the caller).
    static std::unique_ptr<FieldMeta>
        deserialize(unsigned int id, const std::string &serial);

    //read serialized data, deserialize it, and then construct new fieldmeta
    FieldMeta(unsigned int id, const std::string &fname, bool has_salt,
              const std::string &salt_name, onionlayout onion_layout,
              SECURITY_RATING sec_rating, unsigned long uniq_count,
              uint64_t counter, bool has_default,
              const std::string &default_value,
              enum  enum_field_types in_sql_type)
        : MappedDBMeta(id), fname(fname), salt_name(salt_name),
          onion_layout(onion_layout), has_salt(has_salt),
          sec_rating(sec_rating), uniq_count(uniq_count),
          counter(counter), has_default(has_default),
          default_value(default_value),sql_type(in_sql_type) {
    }

    ~FieldMeta() {;}

    std::string serialize(const DBObject &parent) const;

    std::string stringify() const;

    std::vector<std::pair<const OnionMetaKey *, OnionMeta *>>
        orderedOnionMetas() const;

    std::string getSaltName() const;

    unsigned long getUniq() const {return uniq_count;}

    OnionMeta *getOnionMeta(onion o) const;

    TYPENAME("fieldMeta");

    SECURITY_RATING getSecurityRating() const {return sec_rating;}

    bool hasOnion(onion o) const;
    bool hasDefault() const {return has_default;}
    std::string defaultValue() const {return default_value;}
    const onionlayout &getOnionLayout() const {return onion_layout;}
    bool getHasSalt() const {return has_salt;}
    const std::string getFieldName() const {return fname;}

    enum_field_types getSqlType(){return sql_type;}//ADDED BY SHAOYIWEN

private:
    const std::string fname;
    const std::string salt_name;
    const onionlayout onion_layout;
    const bool has_salt; //whether this field has its own salt
    const SECURITY_RATING sec_rating;
    const unsigned long uniq_count;
    uint64_t counter;
    const bool has_default;
    const std::string default_value;

    //added
    enum  enum_field_types sql_type;

    SECLEVEL getOnionLevel(onion o) const;


    static onionlayout determineOnionLayout(const AES_KEY *const m_key,
                                            const Create_field &f,
                                            SECURITY_RATING sec_rating);
    static bool determineHasDefault(const Create_field &cf);

    static std::string determineDefaultValue(bool has_default,
                                             const Create_field &cf);
    uint64_t &getCounter_() {return counter;}
};
class TableMeta : public MappedDBMeta<FieldMeta, IdentityMetaKey>,
                  public UniqueCounter {
public:
    // New TableMeta.
    TableMeta(bool has_sensitive, bool has_salt)
        : hasSensitive(has_sensitive), has_salt(has_salt),
          salt_name("tableSalt_" + getpRandomName()),
          anon_table_name("table_" + getpRandomName()),
          counter(0) {

          }
    // Restore.
    static std::unique_ptr<TableMeta>
        deserialize(unsigned int id, const std::string &serial);
    TableMeta(unsigned int id, const std::string &anon_table_name,
              bool has_sensitive, bool has_salt,
              const std::string &salt_name, unsigned int counter)
        : MappedDBMeta(id), hasSensitive(has_sensitive),
          has_salt(has_salt), salt_name(salt_name),
          anon_table_name(anon_table_name), counter(counter) {
          }
    ~TableMeta() {;}
    std::string serialize(const DBObject &parent) const;
    std::string getAnonTableName() const;
    std::vector<FieldMeta *> orderedFieldMetas() const;
    /* return fieldmeta of fields that has default value */
    std::vector<FieldMeta *> defaultedFieldMetas() const;
    TYPENAME("tableMeta")
    std::string getAnonIndexName(const std::string &index_name,
                                 onion o) const;

private:
    const bool hasSensitive;
    const bool has_salt;
    const std::string salt_name;
    const std::string anon_table_name;
    uint64_t counter;

    uint64_t &getCounter_() {return counter;}
};

class DatabaseMeta : public MappedDBMeta<TableMeta, IdentityMetaKey> {
public:
    // New DatabaseMeta.
    DatabaseMeta() : MappedDBMeta(0) {}
    // Restore.
    static std::unique_ptr<DatabaseMeta>
        deserialize(unsigned int id, const std::string &serial);
    DatabaseMeta(unsigned int id) : MappedDBMeta(id) {}

    ~DatabaseMeta() {}

    std::string serialize(const DBObject &parent) const;
    TYPENAME("databaseMeta")
};

// AWARE: Table/Field aliases __WILL NOT__ be looked up when calling from
// this level or below. Use Analysis::* if you need aliasing.
class SchemaInfo : public MappedDBMeta<DatabaseMeta, IdentityMetaKey> {

public:
    SchemaInfo() : MappedDBMeta(0) {}
    ~SchemaInfo() {}

    TYPENAME("schemaInfo")

private:
    std::string serialize(const DBObject &parent) const
    {
        FAIL_TextMessageError("SchemaInfo can not be serialized!");
    }

};

class SchemaCache {
    SchemaCache(const SchemaCache &cache) = delete;
    SchemaCache &operator=(const SchemaCache &cache) = delete;
    SchemaCache &operator=(SchemaCache &&cache) = delete;

public:
    SchemaCache() : no_loads(true), id(randomValue() % UINT_MAX) {
    }
    SchemaCache(SchemaCache &&cache)
        : schema(std::move(cache.schema)), no_loads(cache.no_loads),
          id(cache.id) {
    }

    std::shared_ptr<const SchemaInfo>
        getSchema(const std::unique_ptr<Connect> &conn,
                  const std::unique_ptr<Connect> &e_conn) const;
    void updateStaleness(const std::unique_ptr<Connect> &e_conn,
                         bool staleness) const;
    bool initialStaleness(const std::unique_ptr<Connect> &e_conn) const;
    bool cleanupStaleness(const std::unique_ptr<Connect> &e_conn) const;
    void lowLevelCurrentStale(const std::unique_ptr<Connect> &e_conn) const;
    void lowLevelCurrentUnstale(const std::unique_ptr<Connect> &e_conn) const;

private:
    mutable std::shared_ptr<const SchemaInfo> schema;
    mutable bool no_loads;
    const unsigned int id;
};

typedef std::shared_ptr<const SchemaInfo> SchemaInfoRef;
