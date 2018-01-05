#include <algorithm>
#include <functional>
#include <memory>

#include <parser/lex_util.hh>
#include <parser/stringify.hh>
#include <parser/mysql_type_metadata.hh>
#include <main/schema.hh>
#include <main/rewrite_main.hh>
#include <main/rewrite_util.hh>
#include <main/dbobject.hh>
#include <main/metadata_tables.hh>
#include <main/macro_util.hh>

//对于schemaInfo而言, 先获得自己的id, 作为parent, 可以查找底下的databasemeta的serial,key以及id
//然后通过lambda表达式,先把databasemeta加入到schemainfo的map中, 然后返回这写个databasemeta供后续使用. 

/*
*for example, we have schemaInfo, then in this function, it first fetch it's own id, and use it as parent
*to fetch its children, the databasemeta. Then it adds those databasemeta as KV in the map, and return those
*databasemeta as a vector for recursive processing.

for this function, it only extract the dbmeta and 
*/
std::vector<DBMeta *>
DBMeta::doFetchChildren(const std::unique_ptr<Connect> &e_conn,
                        std::function<DBMeta *(const std::string &,
                                               const std::string &,
                                               const std::string &)>
                            deserialHandler) {
    
    const std::string table_name = MetaData::Table::metaObject();
    // Now that we know the table exists, SELECT the data we want.
    std::vector<DBMeta *> out_vec;
    std::unique_ptr<DBResult> db_res;
    //this is the id of the current class.
    const std::string parent_id = std::to_string(this->getDatabaseID());
    const std::string serials_query =
        " SELECT " + table_name + ".serial_object,"
        "        " + table_name + ".serial_key,"
        "        " + table_name + ".id"
        " FROM " + table_name +
        " WHERE " + table_name + ".parent_id"
        "   = " + parent_id + ";";
    //all the metadata are fetched here.
    TEST_TextMessageError(e_conn->execute(serials_query, &db_res),
                          "doFetchChildren query failed");
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(db_res->n))) {
        unsigned long * const l = mysql_fetch_lengths(db_res->n);
        assert(l != NULL);
        const std::string child_serial_object(row[0], l[0]);
        const std::string child_key(row[1], l[1]);
        const std::string child_id(row[2], l[2]);
        DBMeta *const new_old_meta =
            deserialHandler(child_key, child_serial_object, child_id);
        out_vec.push_back(new_old_meta);
    }
    return out_vec;
}


//new onion, 这里的uniq_count是通过fieldMeta获得的, 本身没有集成UniqueCounter.
OnionMeta::OnionMeta(onion o, std::vector<SECLEVEL> levels,
                     const AES_KEY * const m_key,
                     const Create_field &cf, unsigned long uniq_count,
                     SECLEVEL minimum_seclevel)
    : onionname(getpRandomName() + TypeText<onion>::toText(o)),
      uniq_count(uniq_count), minimum_seclevel(minimum_seclevel) {
    assert(levels.size() >= 1);

    const Create_field * newcf = &cf;
    //generate enclayers for encrypted field
    const std::string uniqueFieldName = this->getAnonOnionName();
    for (auto l: levels) {
        const std::string key =
            m_key ? getLayerKey(m_key, uniqueFieldName, l)
                  : "plainkey";
        std::unique_ptr<EncLayer>
            el(EncLayerFactory::encLayer(o, l, *newcf, key));

        const Create_field &oldcf = *newcf;
        newcf = el->newCreateField(oldcf);

        this->layers.push_back(std::move(el));
    }
    assert(this->layers.size() >= 1);
}

//onionmeta解序列化
std::unique_ptr<OnionMeta>
OnionMeta::deserialize(unsigned int id, const std::string &serial)
{
    assert(id != 0);
    const auto vec = unserialize_string(serial);
    //OnionMeta序列化的结果有三个.
    assert(3 == vec.size());

    //名字
    const std::string onionname = vec[0];
    //count
    const unsigned int uniq_count = atoi(vec[1].c_str());
    //level
    const SECLEVEL minimum_seclevel = TypeText<SECLEVEL>::toType(vec[2]);

    //在什么位置调用的, id应该是查表获得的id, 为什么需要unique_count?
    return std::unique_ptr<OnionMeta>
        (new OnionMeta(id, onionname, uniq_count, minimum_seclevel));
}

//序列化的结果
std::string OnionMeta::serialize(const DBObject &parent) const
{
    //就是名字,count,以及level分别转换成字符串, 然后用字符串序列化的方法来保存.对于onionmeta,注意name
    const std::string &serial =
        serialize_string(this->onionname) +
        serialize_string(std::to_string(this->uniq_count)) +
        serialize_string(TypeText<SECLEVEL>::toText(this->minimum_seclevel));
    return serial;
}

std::string OnionMeta::getAnonOnionName() const
{
    return onionname;
}

std::vector<DBMeta *>
OnionMeta::fetchChildren(const std::unique_ptr<Connect> &e_conn)
{
    std::function<DBMeta *(const std::string &,
                           const std::string &,
                           const std::string &)>
        deserialHelper =
    [this] (const std::string &key, const std::string &serial,
            const std::string &id) -> EncLayer *
    {
        // > Probably going to want to use indexes in AbstractMetaKey
        // for now, otherwise you will need to abstract and rederive
        // a keyed and nonkeyed version of Delta.
        const std::unique_ptr<UIntMetaKey>
            meta_key(AbstractMetaKey::factory<UIntMetaKey>(key));
        const unsigned int index = meta_key->getValue();
        if (index >= this->layers.size()) {
            this->layers.resize(index + 1);
        }
        std::unique_ptr<EncLayer>
            layer(EncLayerFactory::deserializeLayer(atoi(id.c_str()),
                                                    serial));
        this->layers[index] = std::move(layer);
        return this->layers[index].get();
    };

    return DBMeta::doFetchChildren(e_conn, deserialHelper);
}

bool
OnionMeta::applyToChildren(std::function<bool(const DBMeta &)>
    fn) const
{
    for (const auto &it : layers) {
        if (false == fn(*it.get())) {
            return false;
        }
    }

    return true;
}

UIntMetaKey const &OnionMeta::getKey(const DBMeta &child) const
{
    for (std::vector<EncLayer *>::size_type i = 0; i< layers.size(); ++i) {
        if (&child == layers[i].get()) {
            UIntMetaKey *const key = new UIntMetaKey(i);
            // Hold onto the key so we can destroy it when OnionMeta is
            // destructed.
            generated_keys.push_back(std::unique_ptr<UIntMetaKey>(key));
            return *key;
        }
    }

    assert(false);
}

EncLayer *OnionMeta::getLayerBack() const
{
    TEST_TextMessageError(layers.size() != 0,
                          "Tried getting EncLayer when there are none!");

    return layers.back().get();
}

bool OnionMeta::hasEncLayer(const SECLEVEL &sl) const
{
    for (const auto &it : layers) {
        if (it->level() == sl) {
            return true;
        }
    }

    return false;
}

EncLayer *OnionMeta::getLayer(const SECLEVEL &sl) const
{
    TEST_TextMessageError(layers.size() != 0,
                          "Tried getting EncLayer when there are none!");

    AssignOnce<EncLayer *> out;
    for (const auto &it : layers) {
        if (it->level() == sl) {
            out = it.get();
        }
    }

    assert(out.assigned());
    return out.get();
}

SECLEVEL OnionMeta::getSecLevel() const {
    assert(layers.size() > 0);
    return layers.back()->level();
}


std::unique_ptr<FieldMeta>
FieldMeta::deserialize(unsigned int id, const std::string &serial) {
    assert(id != 0);
    const auto vec = unserialize_string(serial);
    assert(10 == vec.size());//We add one item,so there are ten items now

    const std::string fname = vec[0];
    const bool has_salt = string_to_bool(vec[1]);
    const std::string salt_name = vec[2];
    const onionlayout onion_layout = TypeText<onionlayout>::toType(vec[3]);
    const SECURITY_RATING sec_rating =
        TypeText<SECURITY_RATING>::toType(vec[4]);
    const unsigned int uniq_count = atoi(vec[5].c_str());
    const unsigned int counter = atoi(vec[6].c_str());
    const bool has_default = string_to_bool(vec[7]);
    const std::string default_value = vec[8];
    
    enum  enum_field_types sql_type = ((enum  enum_field_types)atoi(vec[9].c_str()));//new field added

    
    return std::unique_ptr<FieldMeta>
        (new FieldMeta(id, fname, has_salt, salt_name, onion_layout,
                       sec_rating, uniq_count, counter, has_default,
                       default_value,sql_type));
}

// first element is the levels that the onionmeta should implement
// second element is the minimum level that should be gone down too
static std::pair<std::vector<SECLEVEL>, SECLEVEL>
determineSecLevelData(onion o, std::vector<SECLEVEL> levels, bool unique)
{
    if (false == unique) {
        return std::make_pair(levels, levels.front());
    }

    // the oDET onion should start at DET and stay at DET
    if (oDET == o) {
        assert(SECLEVEL::RND == levels.back());
        levels.pop_back();
        assert(SECLEVEL::DET == levels.back());
        return std::make_pair(levels, levels.front());
    }

    // oPLAIN may be starting at PLAINVAL if we have an autoincrement column
    if (oPLAIN == o) {
        assert(SECLEVEL::PLAINVAL == levels.back()
               || SECLEVEL::RND == levels.back());
    } else if (oOPE == o) {
        assert(SECLEVEL::RND == levels.back());
        levels.pop_back();
        assert(SECLEVEL::OPE == levels.back());
        levels.pop_back();
        assert(SECLEVEL::OPEFOREIGN==levels.back());
    } else if (oAGG == o) {
        assert(SECLEVEL::HOM == levels.back());
    } else {
        assert(false);
    }
    return std::make_pair(levels, levels.front());
}

// If mkey == NULL, the field is not encrypted
static bool
init_onions_layout(const AES_KEY *const m_key, FieldMeta *const fm,
                   const Create_field &cf, bool unique)
{
    const onionlayout onion_layout = fm->getOnionLayout();
    if (fm->getHasSalt() != (static_cast<bool>(m_key)
                             && PLAIN_ONION_LAYOUT != onion_layout)) {
        return false;
    }

    if (0 != fm->getChildren().size()) {
        return false;
    }

    for (auto it: onion_layout) {
        const onion o = it.first;
        const std::vector<SECLEVEL> &levels = it.second;

        assert(levels.size() >= 1);
        const std::pair<std::vector<SECLEVEL>, SECLEVEL> level_data =
            determineSecLevelData(o, levels, unique);
        assert(level_data.first.size() >= 1);

        // A new OnionMeta will only occur with a new FieldMeta so
        // we never have to build Deltaz for our OnionMetaz.
        std::unique_ptr<OnionMeta>
            om(new OnionMeta(o, std::get<0>(level_data), m_key, cf,
                             fm->leaseCount(), std::get<1>(level_data)));
        //only for log
        const std::string &onion_name = om->getAnonOnionName();
        fm->addChild(OnionMetaKey(o), std::move(om));

        LOG(cdb_v) << "adding onion layer " << onion_name
                   << " for " << fm->getFieldName();
    }

    return true;
}

FieldMeta::FieldMeta(const Create_field &field,
                     const AES_KEY * const m_key,
                     SECURITY_RATING sec_rating,
                     unsigned long uniq_count,
                     bool unique)
    : fname(std::string(field.field_name)),
      salt_name(BASE_SALT_NAME + getpRandomName()),
      onion_layout(determineOnionLayout(m_key, field, sec_rating)),
      has_salt(static_cast<bool>(m_key)
              && onion_layout != PLAIN_ONION_LAYOUT),
      sec_rating(sec_rating), uniq_count(uniq_count), counter(0),
      has_default(determineHasDefault(field)),
      default_value(determineDefaultValue(has_default, field)) {
    sql_type = field.sql_type;//added by shaoyiwen
    TEST_TextMessageError(init_onions_layout(m_key, this, field, unique),
                          "Failed to build onions for new FieldMeta!");
}

std::string FieldMeta::serialize(const DBObject &parent) const
{
    const std::string &serialized_salt_name =
        true == this->has_salt ? serialize_string(getSaltName())
                               : serialize_string("");
    std::string sql_type_string = std::to_string((int)sql_type);
    const std::string serial =
        serialize_string(fname) +
        serialize_string(bool_to_string(has_salt)) +
        serialized_salt_name +
        serialize_string(TypeText<onionlayout>::toText(onion_layout)) +
        serialize_string(TypeText<SECURITY_RATING>::toText(sec_rating)) +
        serialize_string(std::to_string(uniq_count)) +
        serialize_string(std::to_string(counter)) +
        serialize_string(bool_to_string(has_default)) +
        serialize_string(default_value) +
        serialize_string(sql_type_string);//added by shaoyiwen
   return serial;
}

std::string FieldMeta::stringify() const
{
    const std::string res = " [FieldMeta " + fname + "]";
    return res;
}

//这里FieldMeta的getChildren是pair,OnionMetaKey,OnionMeta, 其中
//onionMeta有根据Uniq排序输出为vector
std::vector<std::pair<const OnionMetaKey *, OnionMeta *>>
FieldMeta::orderedOnionMetas() const
{
    std::vector<std::pair<const OnionMetaKey *, OnionMeta *>> v;
    for (const auto &it : this->getChildren()) {
        v.push_back(std::make_pair(&it.first, it.second.get()));
    }

    std::sort(v.begin(), v.end(),
              [] (std::pair<const OnionMetaKey *, OnionMeta *> a,
                  std::pair<const OnionMetaKey *, OnionMeta *> b) {
                return a.second->getUniq() <
                       b.second->getUniq();
              });

    return v;
}

std::string FieldMeta::getSaltName() const
{
    assert(has_salt);
    return salt_name;
}

SECLEVEL FieldMeta::getOnionLevel(onion o) const
{
    const auto om = getChild(OnionMetaKey(o));
    if (om == NULL) {
        return SECLEVEL::INVALID;
    }

    return om->getSecLevel();
}

OnionMeta *FieldMeta::getOnionMeta(onion o) const
{
    return getChild(OnionMetaKey(o));
}

onionlayout FieldMeta::determineOnionLayout(const AES_KEY *const m_key,
                                            const Create_field &f,
                                            SECURITY_RATING sec_rating)
{
    if (sec_rating == SECURITY_RATING::PLAIN) {
        // assert(!m_key);
        return PLAIN_ONION_LAYOUT;
    }
    TEST_TextMessageError(m_key,
                          "Should be using SECURITY_RATING::PLAIN!");
    if (false == encryptionSupported(f)) {
        //TEST_TextMessageError(SECURITY_RATING::SENSITIVE != sec_rating,
        //                      "A SENSITIVE security rating requires the"
        //                      " field to be supported with cryptography!");
        return PLAIN_ONION_LAYOUT;
    }

    // Don't encrypt AUTO_INCREMENT.
    if (Field::NEXT_NUMBER == f.unireg_check) {
        return PLAIN_ONION_LAYOUT;
    }
    if (SECURITY_RATING::SENSITIVE == sec_rating) {
        if (true == isMySQLTypeNumeric(f)) {
            return NUM_ONION_LAYOUT;
        } else {
            return STR_ONION_LAYOUT;
        }
    } else if (SECURITY_RATING::BEST_EFFORT == sec_rating) {
        if (true == isMySQLTypeNumeric(f)) {
            return BEST_EFFORT_NUM_ONION_LAYOUT;
        } else {
            return BEST_EFFORT_STR_ONION_LAYOUT;
        }
    } else {
        FAIL_TextMessageError("Bad SECURITY_RATING in"
                              " determineOnionLayout(...)!");
    }
}

// mysql is handling default values for fields with implicit defaults that
// allow NULL; these implicit defaults being NULL.
bool FieldMeta::determineHasDefault(const Create_field &cf) {
    return cf.def || cf.flags & NOT_NULL_FLAG;
}

std::string FieldMeta::determineDefaultValue(bool has_default,
                                             const Create_field &cf)
{
    const static std::string zero_string = "'0'";
    const static std::string empty_string = "";

    if (false == has_default) {
        // This value should never be used.
        return empty_string;
    }

    if (cf.def) {
        return ItemToString(*cf.def);
    } else {
        if (true == isMySQLTypeNumeric(cf)) {
            return zero_string;
        } else {
            return empty_string;
        }
    }
}

bool FieldMeta::hasOnion(onion o) const
{
    return childExists(OnionMetaKey(o));
}

std::unique_ptr<TableMeta>
TableMeta::deserialize(unsigned int id, const std::string &serial)
{
    assert(id != 0);
    const auto vec = unserialize_string(serial);
    //table 的解序列化有5个项目.
    assert(5 == vec.size());

    const std::string anon_table_name = vec[0];
    const bool hasSensitive = string_to_bool(vec[1]);
    const bool has_salt = string_to_bool(vec[2]);
    const std::string salt_name = vec[3];
    const unsigned int counter = atoi(vec[4].c_str());

    return std::unique_ptr<TableMeta>
        (new TableMeta(id, anon_table_name, hasSensitive, has_salt,
                       salt_name, counter));
}

//table有5个要素需要进行编码, 匿名的名字, sensitive的bool,salt的bool,salt的名字, 以及counter
//为什么tableMeta和FieldMeta需要继承UniqueCounter
std::string TableMeta::serialize(const DBObject &parent) const
{
    const std::string &serial = 
        serialize_string(getAnonTableName()) +
        serialize_string(bool_to_string(hasSensitive)) +
        serialize_string(bool_to_string(has_salt)) +
        serialize_string(salt_name) +
        serialize_string(std::to_string(counter));
    return serial;
}

// FIXME: May run into problems where a plaintext table expects the regular
// name, but it shouldn't get that name from 'getAnonTableName' anyways.
std::string TableMeta::getAnonTableName() const
{
    return anon_table_name;
}

// FIXME: Slow.
// > Code is also duplicated with FieldMeta::orderedOnionMetas.
std::vector<FieldMeta *> TableMeta::orderedFieldMetas() const
{
    std::vector<FieldMeta *> v;
    for (const auto &it : this->getChildren()) {
        v.push_back(it.second.get());
    }

    std::sort(v.begin(), v.end(),
              [] (FieldMeta *a, FieldMeta *b) {
                return a->getUniq() < b->getUniq();
              }); 

    return v;
}

/*use fm->hasDefault() to test whether the filed has default value*/
std::vector<FieldMeta *> TableMeta::defaultedFieldMetas() const
{
    std::vector<FieldMeta *> v;
    for (const auto &it : this->getChildren()) {
        v.push_back(it.second.get());
    }

    std::vector<FieldMeta *> out_v;
    std::copy_if(v.begin(), v.end(), std::back_inserter(out_v),
                 [] (const FieldMeta *const fm) -> bool
                 {
                    return fm->hasDefault();
                 });

    return out_v;
}

// TODO: Add salt.
std::string TableMeta::getAnonIndexName(const std::string &index_name,
                                        onion o) const
{
    const std::string hash_input =
        anon_table_name + index_name + TypeText<onion>::toText(o);
    const std::size_t hsh = std::hash<std::string>()(hash_input);

    return std::string("index_") + std::to_string(hsh);
}

std::unique_ptr<DatabaseMeta>
DatabaseMeta::deserialize(unsigned int id, const std::string &serial)
{
    assert(id != 0);

    return std::unique_ptr<DatabaseMeta>(new DatabaseMeta(id));
}

std::string
DatabaseMeta::serialize(const DBObject &parent) const
{
    const std::string &serial =
        "Serialize to associate database name with DatabaseMeta";

    return serial;
}

//查询Id对应的stale的值
static bool
lowLevelGetCurrentStaleness(const std::unique_ptr<Connect> &e_conn,
                            unsigned int cache_id)
{
    const std::string &query =
        " SELECT stale FROM " + MetaData::Table::staleness() +
        "  WHERE cache_id = " + std::to_string(cache_id) + ";";
    std::unique_ptr<DBResult> db_res;
    RFIF(e_conn->execute(query, &db_res));
    assert(1 == mysql_num_rows(db_res->n));

    const MYSQL_ROW row = mysql_fetch_row(db_res->n);
    const unsigned long *const l = mysql_fetch_lengths(db_res->n);
    assert(l != NULL);

    return string_to_bool(std::string(row[0], l[0]));
}

std::shared_ptr<const SchemaInfo>
SchemaCache::getSchema(const std::unique_ptr<Connect> &conn,
                       const std::unique_ptr<Connect> &e_conn) const
{
    if (true == this->no_loads) {
        //设置当前id对应的stale的值为true.        
        TEST_SchemaFailure(initialStaleness(e_conn));
        this->no_loads = false;
    }else{
    }

       //查询当前ID对应的Stale的值
    if (true == lowLevelGetCurrentStaleness(e_conn, this->id)) {
        this->schema =
            std::shared_ptr<SchemaInfo>(loadSchemaInfo(conn, e_conn));
    }else{
    }

    assert(this->schema);
    return this->schema;
}

static void
lowLevelAllStale(const std::unique_ptr<Connect> &e_conn)
{
    const std::string &query =
        " UPDATE " + MetaData::Table::staleness() +
        "    SET stale = TRUE;";

    TEST_SchemaFailure(e_conn->execute(query));
}

void
SchemaCache::updateStaleness(const std::unique_ptr<Connect> &e_conn,
                             bool staleness) const
{
    if (true == staleness) {
        // Make everyone stale.
        return lowLevelAllStale(e_conn);
    }

    // We are no longer stale.
    return this->lowLevelCurrentUnstale(e_conn);
}

bool
SchemaCache::initialStaleness(const std::unique_ptr<Connect> &e_conn) const
{
    const std::string seed_staleness =
        " INSERT INTO " + MetaData::Table::staleness() +
        "   (cache_id, stale) VALUES " +
        "   (" + std::to_string(this->id) + ", TRUE);";
    RFIF(e_conn->execute(seed_staleness));

    return true;
}

bool
SchemaCache::cleanupStaleness(const std::unique_ptr<Connect> &e_conn) const
{
    const std::string remove_staleness =
        " DELETE FROM " + MetaData::Table::staleness() +
        "       WHERE cache_id = " + std::to_string(this->id) + ";";
    RFIF(e_conn->execute(remove_staleness));

    return true;
}
static bool
lowLevelToggleCurrentStaleness(const std::unique_ptr<Connect> &e_conn,
                               unsigned int cache_id, bool staleness)
{
    const std::string &query =
        " UPDATE " + MetaData::Table::staleness() +
        "    SET stale = " + bool_to_string(staleness) +
        "  WHERE cache_id = " + std::to_string(cache_id) + ";";
    RFIF(e_conn->execute(query));

    return true;
}

void
SchemaCache::lowLevelCurrentStale(const std::unique_ptr<Connect> &e_conn)
    const
{
    TEST_SchemaFailure(lowLevelToggleCurrentStaleness(e_conn, this->id, true));
}

void
SchemaCache::lowLevelCurrentUnstale(const std::unique_ptr<Connect> &e_conn)
    const
{
    TEST_SchemaFailure(lowLevelToggleCurrentStaleness(e_conn, this->id, false));
}

