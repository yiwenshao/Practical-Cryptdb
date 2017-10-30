#pragma once

#include <functional>
#include <memory>
#include <algorithm>

#include <util/enum_text.hh>
#include <main/serializers.hh>


// FIXME: Maybe should inherit from DBObject.(but it do not need an id??)

/*provide static factory for get a key type*/
class AbstractMetaKey : public NormalAlloc {
public:
    AbstractMetaKey() {;}
    virtual ~AbstractMetaKey() {;}
    virtual std::string getSerial() const = 0;
    /* used in mappedDBmeta: factory<KeyType> */
    template <typename ConcreteKey>
        static ConcreteKey *factory(std::string serial) {
        /*dummy is not used!!*/
        int dummy = 1;
        return new ConcreteKey(dummy, serial);
    }
};

/*
*we have onionMetaKey,UIntMetaKey and IdentityMetaKey for metadata hierachy
*meta key provides KeyType and Serial
*/
template <typename KeyType>
class MetaKey : public AbstractMetaKey {
    const KeyType key_data;
    const std::string serial;

protected:
    // Build MetaKey from serialized MetaKey.
    MetaKey(KeyType key_data, std::string serial) :
        key_data(key_data), serial(serial) {}

public:
    // Build MetaKey from 'actual' key value.
    MetaKey(KeyType key_data) {;}
    virtual ~MetaKey() = 0;
    /*key can be inserted into a map, so it must support those operations*/
    bool operator <(const MetaKey<KeyType> &rhs) const;
    bool operator ==(const MetaKey<KeyType> &rhs) const;

    KeyType getValue() const {return key_data;}
    std::string getSerial() const {return serial;}
};

template <typename KeyType>
MetaKey<KeyType>::~MetaKey() {;}


/*
*this is metakey<string>
*/
class IdentityMetaKey : public MetaKey<std::string> {
public:
    IdentityMetaKey(std::string key_data)
        : MetaKey(key_data, serialize(key_data)) {}
    IdentityMetaKey(int dummy, std::string serial)
        : MetaKey(unserialize(serial), serial) {}
    ~IdentityMetaKey() {;}

private:
    std::string serialize(std::string s)
    {
        return serialize_string(s);
    }

    std::string unserialize(std::string s)
    {
        return unserialize_one_string(s);
    }
};

/*
*this is metakey<onion>
*/
class OnionMetaKey : public MetaKey<onion> {
public:
    OnionMetaKey(onion key_data)
        : MetaKey(key_data, serialize(key_data)) {}
    OnionMetaKey(int dummy, std::string serial)
        : MetaKey(unserialize(serial), serial) {}
    ~OnionMetaKey() {;}

private:
    std::string serialize(onion o)
    {
        return serialize_string(TypeText<onion>::toText(o));
    }

    onion unserialize(std::string s)
    {
        return TypeText<onion>::toType(unserialize_one_string(s));
    }
};


/*
*this is metakey<int>,and it should be noted that onion is different from int, althogh it can be converted
*/
class UIntMetaKey : public MetaKey<unsigned int> {
public:
    UIntMetaKey(unsigned int key_data)
        : MetaKey(key_data, serialize(key_data)) {}
    UIntMetaKey(int dummy, std::string serial)
        : MetaKey(unserialize(serial), serial) {}
    ~UIntMetaKey() {;}

private:
    virtual std::string serialize(unsigned int i){
        return serialize_string(std::to_string(i));
    }

    virtual unsigned int unserialize(std::string s){
        return serial_to_uint(s);
    }
};


/*
* DBObject intends to give each object of this type an 
* id, which can be written into the embeeded database.
*/
class DBObject {
    const unsigned int id;
public:
    // 0 indicates that the object does not have a database id.
    // This is the state of the object before it is written to
    // the database for the first time.
    DBObject() : id(0) {}
    // Unserializing old objects.
    explicit DBObject(unsigned int id) : id(id) {}
    virtual ~DBObject() {;}
    unsigned int getDatabaseID() const {return id;}
};

class Connect;


/*
 * DBMeta is also a design choice about how we use Deltas.
 * i) Read SchemaInfo from database, read Deltaz from database then
 *  apply Deltaz to in memory SchemaInfo.
 *  > How/When do we get an ID the first time we put something into the
 *    SchemaInfo?
 *  > Would likely require a function DBMeta::applyDelta(Delta *)
 *    because we don't have singluarly available interfaces to change
 *    a DBMeta from the outside, ie addChild.
 * ii) Apply Deltaz to SchemaInfo while all is still in database, then
 *  read SchemaInfo from database.
 *  > Logic is in SQL.
 */

class DBMeta : public DBObject, public NormalAlloc {
public:
    DBMeta() {}
    explicit DBMeta(unsigned int id) : DBObject(id) {}
    virtual ~DBMeta() {;}

    // FIXME: Use rtti.
    virtual std::string typeName() const = 0;
    /* */
    virtual std::vector<DBMeta *>
        fetchChildren(const std::unique_ptr<Connect> &e_conn) = 0;
    /* */
    virtual bool
        applyToChildren(std::function<bool(const DBMeta &)>) const = 0;
    /*traverse the map to get the key for the conresponding child(reference MappedDBMeta)*/
    virtual AbstractMetaKey const &getKey(const DBMeta &child) const = 0;
    /*each item in the meta hierachy should be able to serialize,like tablemeta,databasemeta,onionmeta,enclayers*/
    virtual std::string serialize(const DBObject &parent) const = 0;
protected:
    std::vector<DBMeta*>
        doFetchChildren(const std::unique_ptr<Connect> &e_conn,
                        std::function<DBMeta*
                            (const std::string &, const std::string &,
                             const std::string &)>
                            deserialHandler);
};


class LeafDBMeta : public DBMeta {
public:
    LeafDBMeta() {}
    LeafDBMeta(unsigned int id) : DBMeta(id) {}

    /*from DBmeta*/
    std::vector<DBMeta *>
        fetchChildren(const std::unique_ptr<Connect> &e_conn) {
        return std::vector<DBMeta *>();
    }

    /*from DBmeta*/
    bool applyToChildren(std::function<bool(const DBMeta &)>
        fn) const {
        return true;
    }

    /*if this is an abstract function, then enclayers can be acstract classes, which is not correct*/
    AbstractMetaKey const &getKey(const DBMeta &child) const {
        assert(false);
    }
};

// > TODO: Use static deserialization functions for the derived types so we
//   can get rid of the <Constructor>(std::string serial) functions and put
//   'const' back on the members.
// > FIXME: The key in children is a pointer so this means our lookup is
//   slow. Use std::reference_wrapper.

/*
* ChildType is an instance of EncLayer, and KeyType could be MetaKey.
*/
template <typename ChildType, typename KeyType>
class MappedDBMeta : public DBMeta {
public:
    MappedDBMeta() {}
    MappedDBMeta(unsigned int id) : DBMeta(id) {}
    virtual ~MappedDBMeta() {}

    virtual bool addChild(KeyType key, std::unique_ptr<ChildType> meta);
    virtual bool childExists(const KeyType &key) const;    
    virtual ChildType * getChild(const KeyType &key) const;

    /*the return type is different from that of DBMeta, what are the consequences?*/
    KeyType const &getKey(const DBMeta &child) const;

    virtual std::vector<DBMeta *>
        fetchChildren(const std::unique_ptr<Connect> &e_conn);

    /*inherited from DBMeta*/
    bool applyToChildren(std::function<bool(const DBMeta &)> fn) const;

    const std::map<KeyType, std::unique_ptr<ChildType> > &
        getChildren() const {
            return children;
        }
    /*when is this used???*/
    virtual const ChildType *
        getChildWithGChild(const DBMeta &gchild) const;
private:

/*store the hirechy of metadata as a map. For example, each database has sever tables,
*then in the databasemeta stores a map of tables. The keys are of type string(metakey),
*and the values are of type DBMeta.
*/
    std::map<KeyType, std::unique_ptr<ChildType> > children;
};




/*template implemented in dbobject.hh*/
template <typename KeyType>
bool MetaKey<KeyType>::operator <(const MetaKey<KeyType> &rhs) const
{
    return key_data < rhs.key_data;
}

template <typename KeyType>
bool MetaKey<KeyType>::operator ==(const MetaKey<KeyType> &rhs) const{
    return key_data == rhs.key_data;
}

template <typename ChildType, typename KeyType>
bool
MappedDBMeta<ChildType, KeyType>::addChild(KeyType key,
                                           std::unique_ptr<ChildType> meta){
    if (childExists(key)) {
        return false;
    }
    children[key] = std::move(meta);
    return true;
}

template <typename ChildType, typename KeyType>
bool
MappedDBMeta<ChildType, KeyType>::childExists(const KeyType &key) const
{
    return children.end() != children.find(key);
}

// Slow.
// FIXME: Use findChild.
template <typename ChildType, typename KeyType>
ChildType *
MappedDBMeta<ChildType, KeyType>::getChild(const KeyType &key) const
{
    for (const auto &it : children) {
        if (it.first == key) {
            return it.second.get();
        }
    }

    return NULL;
}

// NOTE: Slow.
template <typename ChildType, typename KeyType>
KeyType const &
MappedDBMeta<ChildType, KeyType>::getKey(const DBMeta &child) const{
    for (const auto &it : children) {
        if (it.second.get() == &child) {
            return it.first;
        }
    }
    assert(false);
}

template <typename ChildType, typename KeyType>
std::vector<DBMeta *>
MappedDBMeta<ChildType, KeyType>::fetchChildren(const std::unique_ptr<Connect> &e_conn){
    // Perhaps it's conceptually cleaner to have this lambda return
    // pairs of keys and children and then add the children from local
    // scope.
    std::function<DBMeta *(const std::string &,
                           const std::string &,
                           const std::string &)>
        deserialize =
        [this] (const std::string &key, const std::string &serial,
                const std::string &id)
        {
            const std::unique_ptr<KeyType>
                meta_key(AbstractMetaKey::factory<KeyType>(key));
            //DatabaseMeta::deserialize
            auto dChild = ChildType::deserialize;
            std::unique_ptr<ChildType>
                new_old_meta(dChild(atoi(id.c_str()), serial));
            // Gobble the child.
            this->addChild(*meta_key, std::move(new_old_meta));
            return this->getChild(*meta_key);
        };

    return DBMeta::doFetchChildren(e_conn, deserialize);
}


template <typename ChildType, typename KeyType>
bool
MappedDBMeta<ChildType, KeyType>::applyToChildren(
                            std::function<bool(const DBMeta &)> fn) const{
    for (const auto &it : children) {
        if (false == fn(*it.second.get())) {
            return false;
        }
    }

    return true;
}


template <typename ChildType, typename KeyType>
const ChildType *MappedDBMeta<ChildType, KeyType>::
getChildWithGChild(const DBMeta &gchild) const{
    bool match = false;
    for (const auto &it : this->getChildren()){
        std::function<bool(const DBMeta &)> misGet =              
                    [&it, &gchild, &match] (const DBMeta &possible_match) {
                        if (&possible_match == &gchild) {
                            match = true;
                            return false;       // shortcircuit
                        }

                        return true;
                    };

        it.second->applyToChildren(misGet);
        if (true == match) {
            return it.second.get();
        }
    }
    return nullptr;
}


