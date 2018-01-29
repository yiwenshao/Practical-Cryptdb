#pragma once

#include <algorithm>

#include <util/util.hh>
#include <crypto/prng.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/paillier.hh>
#include <crypto/ope.hh>
#include <crypto/blowfish.hh>
#include <parser/sql_utils.hh>
#include <crypto/SWPSearch.hh>
#include "crypto/ASHE.hh"

#include <main/dbobject.hh>
#include <main/macro_util.hh>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>

#include "parser/mysql_type_metadata.hh"


/* Class hierarchy:
 * EncLayer:
 * -  encrypts and decrypts data for a certain onion layer. It also
 *    knows how to transform the data type of some plain data to the data type of
 *    encrypted data in the DBMS.
 *
 * HOM, SEARCH : more specialized types of EncLayer
 *
 * EncLayerFactory: creates EncLayer-s for SECLEVEL-s of interest
 */


/*
 * TODO:
 *  -- anon name should not be in EncLayers
 *  -- remove unnecessary padding
 */

static std::string
serial_pack(SECLEVEL l, const std::string &name,
            const std::string &layer_info)
{
    return std::to_string(layer_info.length()) + " " + 
           TypeText<SECLEVEL>::toText(l) + " " + name + " " + layer_info;
}

class EncLayer : public LeafDBMeta {
public:
    virtual ~EncLayer() {}
    EncLayer() : LeafDBMeta() {}
    EncLayer(unsigned int id) : LeafDBMeta(id) {}

    TYPENAME("encLayer")

    virtual SECLEVEL level() const = 0;
    virtual std::string name() const = 0;

    // returns a rewritten create field to include in rewritten query
    virtual Create_field *
        newCreateField(const Create_field &cf,
                       const std::string &anonname = "") const = 0;

    virtual Item *encrypt(const Item &ptext, uint64_t IV) const = 0;
    virtual Item *decrypt(const Item &ctext, uint64_t IV) const = 0;

    // returns the decryptUDF to remove the onion layer
    virtual Item *decryptUDF(Item * const col, Item * const ivcol = NULL)
        const
    {
        thrower() << "decryptUDF not supported";
    }

    virtual std::string doSerialize() const = 0;
    std::string serialize(const DBObject &parent) const
    {
        return serial_pack(this->level(), this->name(),
                           this->doSerialize());
    }

protected:
     friend class EncLayerFactory;
};



class DET_str : public EncLayer {
public:
    DET_str(const Create_field &cf, const std::string &seed_key);

    // serialize and deserialize
    std::string doSerialize() const {return rawkey;}
    DET_str(unsigned int id, const std::string &serial);

    virtual SECLEVEL level() const {return SECLEVEL::DET;}
    std::string name() const {return "DET_str";}
    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;
    Item *encrypt(const Item &ptext, uint64_t IV) const;
    Item *decrypt(const Item &ctext, uint64_t IV) const;
    Item * decryptUDF(Item * const col, Item * const ivcol = NULL) const;
protected:
    const std::string rawkey;
    static const int key_bytes = 16;
    static const bool do_pad   = true;
    const std::unique_ptr<const AES_KEY> enckey;
    const std::unique_ptr<const AES_KEY> deckey;
};


class RND_str : public EncLayer {
public:
    RND_str(const Create_field &cf, const std::string &seed_key);

    // serialize and deserialize
    std::string doSerialize() const {return rawkey;}
    RND_str(unsigned int id, const std::string &serial);

    SECLEVEL level() const {return SECLEVEL::RND;}
    std::string name() const {return "RND_str";}
    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;

    Item * encrypt(const Item &ptext, uint64_t IV) const;
    Item * decrypt(const Item &ctext, uint64_t IV) const;
    Item * decryptUDF(Item * const col, Item * const ivcol) const;

private:
    const std::string rawkey;
    static const int key_bytes = 16;
    static const bool do_pad   = true;
    const std::unique_ptr<const AES_KEY> enckey;
    const std::unique_ptr<const AES_KEY> deckey;

};






class HOM : public EncLayer {
public:
    HOM(const Create_field &cf, const std::string &seed_key);
    HOM(const std::string &seed_key);

    // serialize and deserialize
    std::string doSerialize() const {return seed_key;}
    HOM(unsigned int id, const std::string &serial);
    ~HOM();

    SECLEVEL level() const {return SECLEVEL::HOM;}
    std::string name() const {return "HOM";}
    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;

    //TODO needs multi encrypt and decrypt
    Item *encrypt(const Item &p, uint64_t IV) const;
    Item * decrypt(const Item &c, uint64_t IV) const;

    //expr is the expression (e.g. a field) over which to sum
    Item *sumUDA(Item *const expr) const;
    Item *sumUDF(Item *const i1, Item *const i2) const;

protected:
    std::string const seed_key;
    static const uint nbits = 1024;
    mutable Paillier_priv * sk;

private:
    void unwait() const;

    mutable bool waiting;
};

class ASHE : public EncLayer {
public:
    ASHE(const Create_field &cf, const std::string &seed_key):seed_key(seed_key),ashe(1){

    }

    // serialize and deserialize
    std::string doSerialize() const {return seed_key;}
    ASHE(unsigned int id, const std::string &serial);
    ~ASHE();

    SECLEVEL level() const {return SECLEVEL::ASHE;}
    std::string name() const {return "ASHE";}
    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;
    //TODO needs multi encrypt and decrypt
    Item *encrypt(const Item &p, uint64_t IV) const;
    Item * decrypt(const Item &c, uint64_t IV) const;
protected:
    std::string const seed_key;
    mutable RAW_ASHE ashe;
};


class Search : public EncLayer {
public:
    Search(const Create_field &cf, const std::string &seed_key);

    // serialize and deserialize
    std::string doSerialize() const {return key;}
    Search(unsigned int id, const std::string &serial);

    SECLEVEL level() const {return SECLEVEL::SEARCH;}
    std::string name() const {return "SEARCH";}
    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;

    Item *encrypt(const Item &ptext, uint64_t IV) const;
    Item * decrypt(const Item &ctext, uint64_t IV) const
        __attribute__((noreturn));

    //expr is the expression (e.g. a field) over which to sum
    Item * searchUDF(Item * const field, Item * const expr) const;

private:
    static const uint key_bytes = 16;
    std::string const key;
};

extern const std::vector<udf_func*> udf_list;

class EncLayerFactory {
public:
    static std::unique_ptr<EncLayer>
        encLayer(onion o, SECLEVEL sl, const Create_field &cf,
                 const std::string &key);

    // creates EncLayer from its serialization
    static std::unique_ptr<EncLayer>
        deserializeLayer(unsigned int id, const std::string &serial);

    // static std::string serializeLayer(EncLayer * el, DBMeta *parent);
};

class PlainText : public EncLayer {
public:
    PlainText() {}
    PlainText(unsigned int id) : EncLayer(id) {}
    virtual ~PlainText() {;}

    virtual SECLEVEL level() const {return SECLEVEL::PLAINVAL;}
    virtual std::string name() const {return "PLAINTEXT";}

    virtual Create_field *newCreateField(const Create_field &cf,
                                         const std::string &anonname = "")
        const;
    Item *encrypt(const Item &ptext, uint64_t IV) const;
    Item *decrypt(const Item &ctext, uint64_t IV) const;
    Item *decryptUDF(Item * const col, Item * const ivcol = NULL)
        const __attribute__((noreturn));
    std::string doSerialize() const;
};

class CryptedInteger {
public:
    CryptedInteger(const Create_field &cf, const std::string &key)
        : key(key), field_type(cf.sql_type),
          inclusiveRange(supportsRange(cf)) {}
    static CryptedInteger
        deserialize(const std::string &serial);
    CryptedInteger(const std::string &key, enum enum_field_types type,
                   std::pair<int64_t, uint64_t> range)
        : key(key), field_type(type), inclusiveRange(range) {}
    std::string serialize() const;

    void checkValue(uint64_t value) const;
    std::string getKey() const {return key;}
    enum enum_field_types getFieldType() const {return field_type;}
    std::pair<int64_t, uint64_t> getInclusiveRange() const
        { return inclusiveRange; }

private:
    const std::string key;
    const enum enum_field_types field_type;
    const std::pair<int64_t, uint64_t> inclusiveRange;
};



CryptedInteger
overrideCreateFieldCryptedIntegerFactory(const Create_field &cf,
                                         const std::string &key,
                                         signage s,
                                         enum enum_field_types field_type);


class DET_abstract_integer : public EncLayer {
public:
    DET_abstract_integer() : EncLayer() {}
    DET_abstract_integer(unsigned int id)
        : EncLayer(id) {}

    virtual std::string name() const = 0;
    virtual SECLEVEL level() const = 0;

    std::string doSerialize() const;
    template <typename Type>
        static std::unique_ptr<Type>
        deserialize(unsigned int id, const std::string &serial);

    Create_field *newCreateField(const Create_field &cf,
                                 const std::string &anonname = "")
        const;

    // FIXME: final
    Item *encrypt(const Item &ptext, uint64_t IV) const;
    Item *decrypt(const Item &ctext, uint64_t IV) const;
    Item *decryptUDF(Item *const col, Item *const ivcol = NULL) const;

protected:
    static const int bf_key_size = 16;

private:
    std::string getKeyFromSerial(const std::string &serial);
    virtual const CryptedInteger &getCInteger_() const = 0;
    virtual const blowfish &getBlowfish_() const = 0;
};


std::string prng_expand(const std::string &seed_key, uint key_bytes);

class DET_int : public DET_abstract_integer {
public:
    DET_int(const Create_field &cf, const std::string &seed_key)
        : DET_abstract_integer(),
          cinteger(overrideCreateFieldCryptedIntegerFactory(cf,
                                       prng_expand(seed_key, bf_key_size),
                                       signage::UNSIGNED,
                                       MYSQL_TYPE_LONGLONG)),
          bf(cinteger.getKey()) {}

    // create object from serialized contents
    DET_int(unsigned int id, const CryptedInteger &cinteger)
        : DET_abstract_integer(id), cinteger(cinteger), bf(cinteger.getKey()) {}

    virtual SECLEVEL level() const {return SECLEVEL::DET;}
    std::string name() const {return "DET_int";}

private:
    const CryptedInteger cinteger;
    const blowfish bf;

    const CryptedInteger &getCInteger_() const {return cinteger;}
    const blowfish &getBlowfish_() const {return bf;}
};


class OPE_str : public EncLayer {
public:
    OPE_str(const Create_field &cf, const std::string &seed_key);

    // serialize and deserialize
    std::string doSerialize() const {return key;}
    OPE_str(unsigned int id, const std::string &serial);

    SECLEVEL level() const {return SECLEVEL::OPE;}
    std::string name() const {return "OPE_str";}
    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;

    Item *encrypt(const Item &p, uint64_t IV) const;
    Item *decrypt(const Item &c, uint64_t IV) const
        __attribute__((noreturn));

private:
    const std::string key;
    // HACK.
    mutable OPE ope;
    static const size_t key_bytes = 16;
    static const size_t plain_size = 4;
    static const size_t ciph_size = 8;
};

class OPE_int : public EncLayer {
public:
    OPE_int(const Create_field &cf, const std::string &seed_key);
    OPE_int(unsigned int id, const CryptedInteger &cinteger,
            size_t plain_size, size_t ciph_size);
    CryptedInteger opeHelper(const Create_field &f,
                             const std::string &key);

    SECLEVEL level() const {return SECLEVEL::OPE;}
    std::string name() const {return "OPE_int";}

    std::string doSerialize() const;
    static std::unique_ptr<OPE_int>
        deserialize(unsigned int id, const std::string &serial);

    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;

    Item *encrypt(const Item &p, uint64_t IV) const;
    Item *decrypt(const Item &c, uint64_t IV) const;

private:
    const CryptedInteger cinteger;
    static const size_t key_bytes = 16;
    const size_t plain_size;
    const size_t ciph_size;
    mutable OPE ope;                      // HACK
};



class RND_int : public EncLayer {
public:
    RND_int(const Create_field &cf, const std::string &seed_key);
    RND_int(unsigned int id, const CryptedInteger &cinteger);

    std::string doSerialize() const;
    static std::unique_ptr<RND_int>
        deserialize(unsigned int id, const std::string &serial);

    SECLEVEL level() const {return SECLEVEL::RND;}
    std::string name() const {return "RND_int";}

    Create_field * newCreateField(const Create_field &cf,
                                  const std::string &anonname = "")
        const;

    Item *encrypt(const Item &ptext, uint64_t IV) const;
    Item *decrypt(const Item &ctext, uint64_t IV) const;
    Item * decryptUDF(Item * const col, Item * const ivcol) const;

private:
    const CryptedInteger cinteger;
    blowfish const bf;
    static int const key_bytes = 16;
};


