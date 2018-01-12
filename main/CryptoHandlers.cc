#include <main/CryptoHandlers.hh>
#include <main/macro_util.hh>
#include <main/schema.hh>
#include <parser/lex_util.hh>
#include <parser/mysql_type_metadata.hh>
#include <crypto/ope.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/SWPSearch.hh>
#include <crypto/arc4.hh>
#include <util/util.hh>
#include <util/cryptdb_log.hh>
#include <util/zz.hh>

#include <cmath>
#include <memory>

#define LEXSTRING(cstr) { (char*) cstr, sizeof(cstr) }
#define BITS_PER_BYTE 8


using namespace NTL;

#include <utility>

/*
 * Don't try to manually pull values out of Items with Item_int::value
 * because sometimes your 'Item_int', is actually an Item_string and you
 * need to either let MySQL code handle the conversion (Item::val_uint())
 * or you need to handle it yourself (strtoull).
 */

/*
 * ##### STRICT MODE #####
 * if we want to more closely match the semantics of mysql when given an
 * out of range value; we must know at runtime if we are in strict mode
 * or non strict mode.  Then we can reject or cutoff the value depending
 * on the current mode.
 */

/* Implementation class hierarchy is as in .hh file plus:

   - We have a set of implementation EncLayer-s: each implementation layer
   is tied to some specific encryption scheme and key/block size:
   RND_int, RND_str, DET_int, DET_str, OPE_int, OPE_str

   - LayerFactory: creates an implementation EncLayer for a certain security
   level and field type:

   - RNDFactory: outputs a RND layer
         - RND layers: RND_int for blowfish, RND_str for AES

    -DETFactory: outputs a DET layer
         - DET layers: DET_int, DET_str

    -OPEFactory: outputs a OPE layer
         - OPE layers: OPE_int, OPE_str, OPE_dec

    -HOMFactory: outputs a HOM layer
         - HOM layers: HOM (for integers), HOM_dec (for decimals)

 */

//helper functions//

static ZZ
ItemIntToZZ(const Item &ptext)
{
    const ulonglong val = RiboldMYSQL::val_uint(ptext);
    return ZZFromUint64(val);
}

static Item *
ZZToItemInt(const ZZ &val)
{
    const ulonglong v = uint64FromZZ(val);
    return new (current_thd->mem_root) Item_int(v);
}

static Item *
ZZToItemStr(const ZZ &val)
{
    const std::string &str = StringFromZZ(val);
    Item * const newit =
        new (current_thd->mem_root) Item_string(make_thd_string(str),
                                                str.length(),
                                                &my_charset_bin);
    newit->name = NULL; //no alias

    return newit;
}

static ZZ
ItemStrToZZ(const Item &i)
{
    return ZZFromString(ItemToString(i));
}

//============= FACTORIES ==========================//

struct SerialLayer;

class LayerFactory {
public:
    static std::unique_ptr<EncLayer>
    create(const Create_field &cf, const std::string &key) {
        throw "needs to be inherited";
    };
    static std::unique_ptr<EncLayer>
    deserialize(const SerialLayer &serial) {
        throw "needs to be inherited";
    };
};

class RNDFactory : public LayerFactory {
public:
    static std::unique_ptr<EncLayer>
        create(const Create_field &cf, const std::string &key);
    static std::unique_ptr<EncLayer>
        deserialize(unsigned int id, const SerialLayer &serial);
};


class DETFactory : public LayerFactory {
public:
    static std::unique_ptr<EncLayer>
        create(const Create_field &cf, const std::string &key);
    static std::unique_ptr<EncLayer>
        deserialize(unsigned int id, const SerialLayer &serial);
};


class DETJOINFactory : public LayerFactory {
public:
    static std::unique_ptr<EncLayer>
        create(const Create_field &cf, const std::string &key);
    static std::unique_ptr<EncLayer>
        deserialize(unsigned int id, const SerialLayer &serial);
};

class OPEFactory : public LayerFactory {
public:
    static std::unique_ptr<EncLayer>
        create(const Create_field &cf, const std::string &key);
    static std::unique_ptr<EncLayer>
        deserialize(unsigned int id, const SerialLayer &serial);
};

class OPEFOREIGNFactory : public LayerFactory {
public:
    static std::unique_ptr<EncLayer>
        create(const Create_field &cf, const std::string &key);
    static std::unique_ptr<EncLayer>
        deserialize(unsigned int id, const SerialLayer &serial);
};


class HOMFactory : public LayerFactory {
public:
    static std::unique_ptr<EncLayer>
        create(const Create_field &cf, const std::string &key);
    static std::unique_ptr<EncLayer>
        deserialize(unsigned int id, const SerialLayer &serial);
};


//class ASHEFactory : public LayerFactory {
//public:
//    static std::unique_ptr<EncLayer>
//        create(const Create_field &cf, const std::string &key);
//    static std::unique_ptr<EncLayer>
//        deserialize(unsigned int id, const SerialLayer &serial);
//};


/*=====================  SERIALIZE Helpers =============================*/

struct SerialLayer {
    SECLEVEL l;
    std::string name;
    std::string layer_info;
};

static SerialLayer
serial_unpack(std::string serial)
{
    SerialLayer sl;

    std::stringstream ss(serial);
    uint len;
    ss >> len;
    std::string levelname;
    ss >> levelname;
    sl.l = TypeText<SECLEVEL>::toType(levelname);
    ss >> sl.name;
    sl.layer_info = serial.substr(serial.size()-len, len);

    return sl;
}

// ============================ Factory implementations ====================//


std::unique_ptr<EncLayer>
EncLayerFactory::encLayer(onion o, SECLEVEL sl, const Create_field &cf,
                          const std::string &key)
{
    switch (sl) {
        case SECLEVEL::RND: {return RNDFactory::create(cf, key);}
        case SECLEVEL::DET: {return DETFactory::create(cf, key);}
        case SECLEVEL::DETJOIN: {return DETJOINFactory::create(cf, key);}
        case SECLEVEL::OPE:{return OPEFactory::create(cf, key);}
        case SECLEVEL::OPEFOREIGN:{return OPEFOREIGNFactory::create(cf,key);}
        case SECLEVEL::HOM: {return HOMFactory::create(cf, key);}
        case SECLEVEL::ASHE: {return std::unique_ptr<EncLayer>(new ASHE(cf,key));}
        case SECLEVEL::SEARCH: {
            return std::unique_ptr<EncLayer>(new Search(cf, key));
        }
        case SECLEVEL::PLAINVAL: {
            return std::unique_ptr<EncLayer>(new PlainText());
        }
        default:{}
    }
    FAIL_TextMessageError("unknown or unimplemented security level");
}

//recover from the database using lambda.
std::unique_ptr<EncLayer>
EncLayerFactory::deserializeLayer(unsigned int id,
                                  const std::string &serial){
    assert(id);
    const SerialLayer li = serial_unpack(serial);

    switch (li.l) {
        case SECLEVEL::RND:
            return RNDFactory::deserialize(id, li);

        case SECLEVEL::DET:
            return DETFactory::deserialize(id, li);

        case SECLEVEL::DETJOIN:
            return DETJOINFactory::deserialize(id, li);

        case SECLEVEL::OPEFOREIGN:
            return OPEFOREIGNFactory::deserialize(id,li);

        case SECLEVEL::OPE:
            return OPEFactory::deserialize(id, li);

        case SECLEVEL::HOM:
            return std::unique_ptr<EncLayer>(new HOM(id, serial));
        case SECLEVEL::ASHE: return std::unique_ptr<EncLayer>(new ASHE(id, serial));

        case SECLEVEL::SEARCH:
            return std::unique_ptr<EncLayer>(new Search(id, serial));

        case SECLEVEL::PLAINVAL:
            return std::unique_ptr<EncLayer>(new PlainText(id));

        default:{}
    }
    FAIL_TextMessageError("unknown or unimplemented security level");
}

/*
string
EncLayerFactory::serializeLayer(EncLayer * el, DBMeta *parent) {
    return serial_pack(el->level(), el->name(), el->serialize(*parent));
}
*/

/* ========================= other helpers ============================*/

static
std::string prng_expand(const std::string &seed_key, uint key_bytes)
{
    streamrng<arc4> prng(seed_key);
    return prng.rand_string(key_bytes);
}

//TODO: remove above newcreatefield
static Create_field*
lowLevelcreateFieldHelper(const Create_field &f,
                          unsigned long field_length,
                          enum enum_field_types type,
                          const std::string &anonname = "",
                          CHARSET_INFO * const charset = NULL)
{
    //从内存分配新的Create_field
    const THD * const thd = current_thd;
    Create_field * const f0 = f.clone(thd->mem_root);
    f0->length = field_length;
    f0->sql_type = type;

    if (charset != NULL) {
        f0->charset = charset;
    }

    if (anonname.size() > 0) {
        f0->field_name = make_thd_string(anonname);
    }

    return f0;
}

static Create_field*
integerCreateFieldHelper(const Create_field &f,
                         enum enum_field_types type,
                         const std::string &anonname = "",
                         CHARSET_INFO * const charset = NULL){
    return lowLevelcreateFieldHelper(f, 0, type, anonname, charset);
}

static Create_field*
arrayCreateFieldHelper(const Create_field &f,
                       unsigned long field_length,
                       enum enum_field_types type,
                       const std::string &anonname = "",
                       CHARSET_INFO * const charset = NULL){
    return lowLevelcreateFieldHelper(f, field_length, type, anonname, charset);
}

static Item *
get_key_item(const std::string &key)
{
    Item_string * const keyI =
        new Item_string(make_thd_string(key), key.length(),
                        &my_charset_bin);
    keyI->name = NULL; // no alias
    return keyI;
}

// Can only check unsigned values
static bool
rangeCheck(uint64_t value, std::pair<int64_t, uint64_t> inclusiveRange)
{
    const int64_t minimum  = inclusiveRange.first;
    const uint64_t maximum = inclusiveRange.second;

    // test lower bound
    const bool b = minimum < 0 || static_cast<uint64_t>(minimum) <= value;

    // test upper bound
    return b && value <= maximum;
}

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



static CryptedInteger
overrideCreateFieldCryptedIntegerFactory(const Create_field &cf,
                                         const std::string &key,
                                         signage s,
                                         enum enum_field_types field_type) {
    // the override (@s and @field_type) should support the default range
    // in @cf

    // create a Create_field object so we can build metadata
    Create_field *const override_cf = cf.clone(current_thd->mem_root);
    override_cf->sql_type = field_type;
    override_cf->flags    =
        override_cf->flags
        & (signage::UNSIGNED == s ? UNSIGNED_FLAG : ~UNSIGNED_FLAG);

    // get the range for the field constructed by the user
    const std::pair<int64_t, uint64_t> userInclusiveRange =
        supportsRange(cf);

    // get the range for the overriding type
    const std::pair<int64_t, uint64_t> overrideInclusiveRange =
        supportsRange(*override_cf);

    // do we have a fit?
    TEST_Text(userInclusiveRange.first >= overrideInclusiveRange.first
           && userInclusiveRange.second <= overrideInclusiveRange.second,
              "The field you are trying to create with type "
              + TypeText<enum enum_field_types>::toText(cf.sql_type)
              + " could not be overridden properly");

    return CryptedInteger(key, override_cf->sql_type,
                          overrideInclusiveRange);
}

CryptedInteger
CryptedInteger::deserialize(const std::string &serial) {
    const std::vector<std::string> &vec = unserialize_string(serial);
    const std::string &key = vec[0];
    const enum enum_field_types field_type =
        TypeText<enum enum_field_types>::toType(vec[1]);
    const int64_t inclusiveMinimum = strtoll(vec[2].c_str(), NULL, 0);
    const int64_t inclusiveMaximum = strtoull(vec[3].c_str(), NULL, 0);

    return CryptedInteger(key, field_type,
                          std::make_pair(inclusiveMinimum,
                                         inclusiveMaximum));
}

void
CryptedInteger::checkValue(uint64_t value) const
{
    TEST_Text(rangeCheck(value, inclusiveRange),
              "can't handle out of range value!");
}

static std::string
serializeStrings(std::initializer_list<std::string> inputs)
{
    std::string out;
    for (auto it : inputs) {
        out += serialize_string(it);
    }

    return out;
}

std::string
CryptedInteger::serialize() const
{
    return serializeStrings({key,
            TypeText<enum enum_field_types>::toText(field_type),
            std::to_string(inclusiveRange.first),
            std::to_string(inclusiveRange.second)});
}


/*********************** RND ************************************************/

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

static unsigned long long
strtoul_(const std::string &s)
{
    return strtoul(s.c_str(), NULL, 0);
}

std::unique_ptr<EncLayer>
RNDFactory::create(const Create_field &cf, const std::string &key)
{
    if (isMySQLTypeNumeric(cf)) {
        return std::unique_ptr<EncLayer>(new RND_int(cf, key));
    } else {
        return std::unique_ptr<EncLayer>(new RND_str(cf, key));
    }
}

std::unique_ptr<EncLayer>
RNDFactory::deserialize(unsigned int id, const SerialLayer &sl)
{
    if (sl.name == "RND_int") {
        return RND_int::deserialize(id, sl.layer_info);
    } else {
        return std::unique_ptr<EncLayer>(new RND_str(id, sl.layer_info));
    }
}


RND_int::RND_int(const Create_field &f, const std::string &seed_key)
    : EncLayer(),
      cinteger(overrideCreateFieldCryptedIntegerFactory(f,
                                         prng_expand(seed_key, key_bytes),
                                         signage::UNSIGNED,
                                         MYSQL_TYPE_LONGLONG)),
      bf(cinteger.getKey())
{}

RND_int::RND_int(unsigned int id, const CryptedInteger &cinteger)
    : EncLayer(id), cinteger(cinteger), bf(cinteger.getKey())
{}

std::string
RND_int::doSerialize() const
{
    return cinteger.serialize();
}

std::unique_ptr<RND_int>
RND_int::deserialize(unsigned int id, const std::string &serial)
{
    const CryptedInteger cint = CryptedInteger::deserialize(serial);
    return std::unique_ptr<RND_int>(new RND_int(id, cint));
}

Create_field *
RND_int::newCreateField(const Create_field &cf,
                        const std::string &anonname) const
{
    return integerCreateFieldHelper(cf, cinteger.getFieldType(), anonname);
}

//TODO: may want to do more specialized crypto for lengths
Item *
RND_int::encrypt(const Item &ptext, uint64_t IV) const
{
    //TODO: should have encrypt_SEM work for any length
    const uint64_t p = RiboldMYSQL::val_uint(ptext);
    cinteger.checkValue(p);

    const uint64_t c = bf.encrypt(p ^ IV);
    LOG(encl) << "RND_int encrypt " << p << " IV " << IV << "-->" << c;

    return new (current_thd->mem_root)
               Item_int(static_cast<ulonglong>(c));
}

Item *
RND_int::decrypt(const Item &ctext, uint64_t IV) const
{
    const uint64_t c = static_cast<const Item_int &>(ctext).value;
    const uint64_t p = bf.decrypt(c) ^ IV;
    LOG(encl) << "RND_int decrypt " << c << " IV " << IV << " --> " << p;

    return new (current_thd->mem_root)
               Item_int(static_cast<ulonglong>(p));
}

static udf_func u_decRNDInt = {
    LEXSTRING("cryptdb_decrypt_int_sem"),
    INT_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};


Item *
RND_int::decryptUDF(Item * const col, Item * const ivcol) const
{
    List<Item> l;
    l.push_back(col);

    l.push_back(get_key_item(cinteger.getKey()));

    l.push_back(ivcol);

    Item *const udfdec =
        new (current_thd->mem_root) Item_func_udf_int(&u_decRNDInt, l);
    udfdec->name = NULL; //no alias

    //add encompassing CAST for unsigned
    Item *const udf =
        new (current_thd->mem_root) Item_func_unsigned(udfdec);
    udf->name = NULL;

    return udf;
}

///////////////////////////////////////////////

RND_str::RND_str(const Create_field &f, const std::string &seed_key)
    : EncLayer(), rawkey(prng_expand(seed_key, key_bytes)),
      enckey(get_AES_enc_key(rawkey)), deckey(get_AES_dec_key(rawkey))
{}

RND_str::RND_str(unsigned int id, const std::string &serial)
    : EncLayer(id), rawkey(serial), enckey(get_AES_enc_key(rawkey)),
      deckey(get_AES_dec_key(rawkey))
{}


Create_field *
RND_str::newCreateField(const Create_field &cf,
                        const std::string &anonname) const
{
    const auto typelen = AESTypeAndLength(cf, do_pad);
    return arrayCreateFieldHelper(cf, typelen.second, typelen.first,
                                  anonname, &my_charset_bin);
}

Item *
RND_str::encrypt(const Item &ptext, uint64_t IV) const
{
    const std::string &enc =
        encrypt_AES_CBC(ItemToString(ptext), enckey.get(),
                        BytesFromInt(IV, SALT_LEN_BYTES), do_pad);

    LOG(encl) << "RND_str encrypt " << ItemToString(ptext) << " IV "
              << IV << "--->" << "len of enc " << enc.length()
              << " enc " << enc;

    return new (current_thd->mem_root) Item_string(make_thd_string(enc),
                                                   enc.length(),
                                                   &my_charset_bin);
}

Item *
RND_str::decrypt(const Item &ctext, uint64_t IV) const
{
    const std::string &dec =
        decrypt_AES_CBC(ItemToString(ctext), deckey.get(),
                        BytesFromInt(IV, SALT_LEN_BYTES), do_pad);
    LOG(encl) << "RND_str decrypt " << ItemToString(ctext) << " IV "
              << IV << "-->" << "len of dec " << dec.length()
              << " dec: " << dec;

    return new (current_thd->mem_root) Item_string(make_thd_string(dec),
                                                   dec.length(),
                                                   &my_charset_bin);
}


//TODO; make edb.cc udf naming consistent with these handlers
static udf_func u_decRNDString = {
    LEXSTRING("cryptdb_decrypt_text_sem"),
    STRING_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};


Item *
RND_str::decryptUDF(Item * const col, Item * const ivcol) const
{
    List<Item> l;
    l.push_back(col);
    l.push_back(get_key_item(rawkey));
    l.push_back(ivcol);

    return new (current_thd->mem_root) Item_func_udf_str(&u_decRNDString,
                                                         l);
}


/********** DET ************************/


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

static udf_func u_decDETInt = {
    LEXSTRING("cryptdb_decrypt_int_det"),
    INT_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
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


std::unique_ptr<EncLayer>
DETFactory::create(const Create_field &cf, const std::string &key)
{
    if (isMySQLTypeNumeric(cf)) {
        if (cf.sql_type == MYSQL_TYPE_DECIMAL
            || cf.sql_type == MYSQL_TYPE_NEWDECIMAL) {
            FAIL_TextMessageError("decimal support is broken");
        } else {
            return std::unique_ptr<EncLayer>(new DET_int(cf, key));
        }
    } else {
        return std::unique_ptr<EncLayer>(new DET_str(cf, key));
    }
}

std::unique_ptr<EncLayer>
DETFactory::deserialize(unsigned int id, const SerialLayer &sl)
{
    if ("DET_int" == sl.name) {
        return DET_abstract_integer::deserialize<DET_int>(id,
                                                       sl.layer_info);
    } else if ("DET_dec" == sl.name) {
        FAIL_TextMessageError("decimal support broken");
    } else if ("DET_str" == sl.name) {
        return std::unique_ptr<EncLayer>(new DET_str(id, sl.layer_info));
    } else {
        FAIL_TextMessageError("Unknown type for DET deserialization!");
    }
}

std::string
DET_abstract_integer::doSerialize() const
{
    return getCInteger_().serialize();
}

template <typename Type>
std::unique_ptr<Type>
DET_abstract_integer::deserialize(unsigned int id,
                                  const std::string &serial)
{
    /* if the concrete DET integer classes need to serialize data that
     * is not in CryptedInteger; write a deserialize function for them
     * as well and let them handle the serialized data that is not
     * CryptedInteger */
    const CryptedInteger &cint = CryptedInteger::deserialize(serial);
    return std::unique_ptr<Type>(new Type(id, cint));
}

Create_field *
DET_abstract_integer::newCreateField(const Create_field &cf,
                                     const std::string &anonname) const
{
    return integerCreateFieldHelper(cf, getCInteger_().getFieldType(),
                                    anonname);
}

Item *
DET_abstract_integer::encrypt(const Item &ptext, uint64_t IV) const
{
    const ulonglong value = RiboldMYSQL::val_uint(ptext);
    getCInteger_().checkValue(value);

    const ulonglong res = static_cast<ulonglong>(getBlowfish_().encrypt(value));
    LOG(encl) << "DET_int enc " << value << "--->" << res;
    return new (current_thd->mem_root) Item_int(res);
}

Item *
DET_abstract_integer::decrypt(const Item &ctext, uint64_t IV) const
{
    const ulonglong value = static_cast<const Item_int &>(ctext).value;
    const ulonglong retdec = getBlowfish_().decrypt(value);
    LOG(encl) << "DET_int dec " << value << "--->" << retdec;
    return new (current_thd->mem_root) Item_int(retdec);
}

Item *
DET_abstract_integer::decryptUDF(Item *const col, Item *const ivcol)
    const
{
    List<Item> l;
    l.push_back(col);

    l.push_back(get_key_item(getCInteger_().getKey()));

    Item *const udfdec = new Item_func_udf_int(&u_decDETInt, l);
    udfdec->name = NULL;

    Item *const udf = new Item_func_unsigned(udfdec);
    udf->name = NULL;

    return udf;
}

std::string
DET_abstract_integer::getKeyFromSerial(const std::string &serial)
{
    return serial.substr(serial.find(' ')+1, std::string::npos);
}

DET_str::DET_str(const Create_field &f, const std::string &seed_key)
    : rawkey(prng_expand(seed_key, key_bytes)),
      enckey(get_AES_enc_key(rawkey)), deckey(get_AES_dec_key(rawkey))
{}

DET_str::DET_str(unsigned int id, const std::string &serial)
    : EncLayer(id), rawkey(serial), enckey(get_AES_enc_key(rawkey)),
    deckey(get_AES_dec_key(rawkey))
{}


Create_field *
DET_str::newCreateField(const Create_field &cf,
                        const std::string &anonname) const
{
    const auto typelen = AESTypeAndLength(cf, do_pad);
    return arrayCreateFieldHelper(cf, typelen.second, typelen.first,
                                  anonname, &my_charset_bin);
}

Item *
DET_str::encrypt(const Item &ptext, uint64_t IV) const
{
    const std::string plain = ItemToString(ptext);
    const std::string enc = encrypt_AES_CMC(plain, enckey.get(), do_pad);
    LOG(encl) << " DET_str encrypt " << plain  << " IV " << IV << " ---> "
              << " enc len " << enc.length() << " enc " << enc;

    return new (current_thd->mem_root) Item_string(make_thd_string(enc),
                                                   enc.length(),
                                                   &my_charset_bin);
}

Item *
DET_str::decrypt(const Item &ctext, uint64_t IV) const
{
    const std::string enc = ItemToString(ctext);
    const std::string dec = decrypt_AES_CMC(enc, deckey.get(), do_pad);
    LOG(encl) << " DET_str decrypt enc len " << enc.length()
              << " enc " << enc << " IV " << IV << " ---> "
              << " dec len " << dec.length() << " dec " << dec;

    return new (current_thd->mem_root) Item_string(make_thd_string(dec),
                                                   dec.length(),
                                                   &my_charset_bin);
}

static udf_func u_decDETStr = {
    LEXSTRING("cryptdb_decrypt_text_det"),
    STRING_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};



Item *
DET_str::decryptUDF(Item * const col, Item * const ivcol) const
{
    List<Item> l;
    l.push_back(col);
    l.push_back(get_key_item(rawkey));
    return new (current_thd->mem_root) Item_func_udf_str(&u_decDETStr, l);

}

/*************** DETJOIN *********************/


class DETJOIN_int : public DET_abstract_integer {
public:
    // blowfish always produces 64 bit output so we should always use
    // unsigned MYSQL_TYPE_LONGLONG
    DETJOIN_int(const Create_field &cf, const std::string &seed_key)
    : DET_abstract_integer(),
      cinteger(overrideCreateFieldCryptedIntegerFactory(cf,
                                       prng_expand(seed_key, bf_key_size),
                                       signage::UNSIGNED,
                                       MYSQL_TYPE_LONGLONG)),
      bf(cinteger.getKey()) {}

    // serialize from parent;  unserialize:
    DETJOIN_int(unsigned int id, const CryptedInteger &cinteger)
        : DET_abstract_integer(id), cinteger(cinteger), bf(cinteger.getKey()) {}

    SECLEVEL level() const {return SECLEVEL::DETJOIN;}
    std::string name() const {return "DETJOIN_int";}

private:
    const CryptedInteger cinteger;
    const blowfish bf;

    const CryptedInteger &getCInteger_() const {return cinteger;}
    const blowfish &getBlowfish_() const {return bf;}
};

class DETJOIN_str : public DET_str {
public:
    DETJOIN_str(const Create_field &cf, const std::string &seed_key)
        : DET_str(cf, seed_key) {}

    // serialize from parent; unserialize:
    DETJOIN_str(unsigned int id, const std::string &serial)
        : DET_str(id, serial) {};

    SECLEVEL level() const {return SECLEVEL::DETJOIN;}
    std::string name() const {return "DETJOIN_str";}
};

std::unique_ptr<EncLayer>
DETJOINFactory::create(const Create_field &cf,
                       const std::string &key)
{
    if (isMySQLTypeNumeric(cf)) {
        if (cf.sql_type == MYSQL_TYPE_DECIMAL
            || cf.sql_type == MYSQL_TYPE_NEWDECIMAL) {
            FAIL_TextMessageError("decimal support is broken");
        } else {
            return std::unique_ptr<EncLayer>(new DETJOIN_int(cf, key));
        }
    } else {
        return std::unique_ptr<EncLayer>(new DETJOIN_str(cf, key));
    }
}

std::unique_ptr<EncLayer>
DETJOINFactory::deserialize(unsigned int id, const SerialLayer &sl)
{
    if ("DETJOIN_int" == sl.name) {
        return DET_abstract_integer::deserialize<DETJOIN_int>(id,
                                                    sl.layer_info);
    } else if ("DETJOIN_dec" == sl.name) {
        FAIL_TextMessageError("decimal support broken");
    } else if ("DETJOIN_str" == sl.name) {
        return std::unique_ptr<EncLayer>(new DETJOIN_str(id,
                                                         sl.layer_info));
    } else {
        FAIL_TextMessageError("DETJOINFactory does not recognize type!");
    }
}



/**************** OPE **************************/


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

class OPEFOREIGN_int:public OPE_int{
public:
    OPEFOREIGN_int(const Create_field &cf, const std::string &seed_key):OPE_int(cf,seed_key){}
    OPEFOREIGN_int(unsigned int id, const CryptedInteger &cinteger,
            size_t plain_size, size_t ciph_size):OPE_int(id,cinteger,plain_size,ciph_size){}
    SECLEVEL level() const {return SECLEVEL::OPEFOREIGN;}
    std::string name() const {return "OPEFOREIGN_int";}
    static std::unique_ptr<OPEFOREIGN_int>
        deserialize(unsigned int id, const std::string &serial);
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

class OPEFOREIGN_str: public OPE_str{
public:
    OPEFOREIGN_str(const Create_field &cf, const std::string &seed_key):OPE_str(cf,seed_key){}
    OPEFOREIGN_str(unsigned int id, const std::string &serial):OPE_str(id,serial){}

    SECLEVEL level() const {return SECLEVEL::OPEFOREIGN;}
    std::string name() const {return "OPEFOREIGN_str";}

};


std::unique_ptr<EncLayer>
OPEFactory::create(const Create_field &cf, const std::string &key)
{
    if (isMySQLTypeNumeric(cf)) {
        if (cf.sql_type == MYSQL_TYPE_DECIMAL
            || cf.sql_type ==  MYSQL_TYPE_NEWDECIMAL) {
            FAIL_TextMessageError("decimal support is broken");
        }
        return std::unique_ptr<EncLayer>(new OPE_int(cf, key));
    }
    return std::unique_ptr<EncLayer>(new OPE_str(cf, key));
}

std::unique_ptr<EncLayer>
OPEFactory::deserialize(unsigned int id, const SerialLayer &sl)
{
    if (sl.name == "OPE_int") {
        return OPE_int::deserialize(id, sl.layer_info);
    } else if (sl.name == "OPE_str") {
        return std::unique_ptr<EncLayer>(new OPE_str(id, sl.layer_info));
    } else {
        FAIL_TextMessageError("decimal support broken");
    }
}


std::unique_ptr<EncLayer>
OPEFOREIGNFactory::create(const Create_field &cf, const std::string &key) {
    if (isMySQLTypeNumeric(cf)) {
        if (cf.sql_type == MYSQL_TYPE_DECIMAL
            || cf.sql_type ==  MYSQL_TYPE_NEWDECIMAL) {
            FAIL_TextMessageError("decimal support is broken");
        }
        return std::unique_ptr<EncLayer>(new OPEFOREIGN_int(cf, key));
    }
    return std::unique_ptr<EncLayer>(new OPEFOREIGN_str(cf, key));
}

std::unique_ptr<EncLayer>
OPEFOREIGNFactory::deserialize(unsigned int id, const SerialLayer &sl)
{
    if (sl.name == "OPEFOREIGN_int") {
        return OPEFOREIGN_int::deserialize(id, sl.layer_info);
    } else if (sl.name == "OPEFOREIGN_str") {
        return std::unique_ptr<EncLayer>(new OPEFOREIGN_str(id, sl.layer_info));
    } else {
        FAIL_TextMessageError("decimal support broken");
    }
}





static size_t
toMultiple(size_t n, size_t multiple)
{
    assert(multiple > 0);

    const size_t remainder = n % multiple;
    if (0 == remainder) {
        return n;
    }

    return n + (multiple - remainder);
}

/*
 * OPE_int::opeHelper(...), opePlainSize(...) and opeCiphSize(...) must all
 * play nice as OPE_int::opeHelper(...) assumes the doubling of field size
 * when it decides if a VARCHAR field is necessary, but this actual doubling
 * doesn't occur until opeCiphSize(...).
 */
CryptedInteger
OPE_int::opeHelper(const Create_field &f, const std::string &key)
{
    const auto plain_inclusive_range = supportsRange(f);
    assert(0 == plain_inclusive_range.first);

    // these fields can not be represented with 64 bits; HACK
    if (plain_inclusive_range.second > 0xFFFFFFFF) {
        return CryptedInteger(key, MYSQL_TYPE_VARCHAR,
                              plain_inclusive_range);
    }

    const auto crypto_inclusive_range =
        std::make_pair(0, plain_inclusive_range.second
                          * (2 + plain_inclusive_range.second));
    // FIXME: pass Create_field object so we can account for signage
    const std::pair<bool, enum enum_field_types> field_type =
        getTypeForRange(crypto_inclusive_range);
    TEST_Text(true == field_type.first,
              "could not build an OPE onion for field type: "
              + TypeText<enum enum_field_types>::toText(f.sql_type) + "\n");

    return CryptedInteger(key, field_type.second, plain_inclusive_range);
}

static size_t
opePlainSize(const CryptedInteger &cinteger)
{
    return toMultiple(log2(cinteger.getInclusiveRange().second),
                           BITS_PER_BYTE)
           / BITS_PER_BYTE;
}

// we need twice as many bytes for the cipher; this means that we
// don't actually use the field type that the user specified
static size_t
opeCiphSize(const CryptedInteger &cinteger)
{
    return 2 * opePlainSize(cinteger);
}

OPE_int::OPE_int(const Create_field &f, const std::string &seed_key)
    : cinteger(opeHelper(f, prng_expand(seed_key, key_bytes))),
      plain_size(opePlainSize(cinteger)), ciph_size(opeCiphSize(cinteger)),
      ope(OPE(cinteger.getKey(), plain_size * BITS_PER_BYTE,
              ciph_size * BITS_PER_BYTE))
{}

OPE_int::OPE_int(unsigned int id, const CryptedInteger &cinteger,
                 size_t plain_size, size_t ciph_size)
    : EncLayer(id), cinteger(cinteger), plain_size(plain_size),
      ciph_size(ciph_size),
      ope(OPE(cinteger.getKey(), plain_size * BITS_PER_BYTE,
              ciph_size * BITS_PER_BYTE))
{}

std::unique_ptr<OPE_int>
OPE_int::deserialize(unsigned int id, const std::string &serial)
{
    const std::vector<std::string> vec = unserialize_string(serial);
    const size_t plain_bytes = strtoul_(vec[0]);
    const size_t ciph_bytes  = strtoul_(vec[1]);
    const CryptedInteger cint = CryptedInteger::deserialize(vec[2]);
    return std::unique_ptr<OPE_int>(new OPE_int(id, cint, plain_bytes,
                                                ciph_bytes) );
}


std::unique_ptr<OPEFOREIGN_int>
OPEFOREIGN_int::deserialize(unsigned int id, const std::string &serial)
{
    const std::vector<std::string> vec = unserialize_string(serial);
    const size_t plain_bytes = strtoul_(vec[0]);
    const size_t ciph_bytes  = strtoul_(vec[1]);
    const CryptedInteger cint = CryptedInteger::deserialize(vec[2]);
    return std::unique_ptr<OPEFOREIGN_int>(new OPEFOREIGN_int(id, cint, plain_bytes,
                                                ciph_bytes));
}


std::string
OPE_int::doSerialize() const
{
    return serializeStrings({std::to_string(plain_size),
                             std::to_string(ciph_size),
                             cinteger.serialize()});
}

Create_field *
OPE_int::newCreateField(const Create_field &cf,
                        const std::string &anonname) const
{
    if (isMySQLTypeNumeric(cinteger.getFieldType())) {
        return integerCreateFieldHelper(cf, cinteger.getFieldType(),
                                        anonname);
    }

    // create a varbinary column because we could not map the user's
    // desired field type into an integer column
    assert(MYSQL_TYPE_VARCHAR == cinteger.getFieldType());
    return arrayCreateFieldHelper(cf, ciph_size, cinteger.getFieldType(),
                                  anonname, &my_charset_bin);
}

static std::string
reverse(const std::string &s)
{
    return std::string(s.rbegin(), s.rend());
}

Item *
OPE_int::encrypt(const Item &ptext, uint64_t IV) const
{
    const uint64_t pval = RiboldMYSQL::val_uint(ptext);
    cinteger.checkValue(pval);

    LOG(encl) << "OPE_int encrypt " << pval << " IV " << IV << std::endl;

    if (MYSQL_TYPE_VARCHAR != this->cinteger.getFieldType()) {
        const ulonglong enc = uint64FromZZ(ope.encrypt(ZZFromUint64(pval)));
        return new Item_int(enc);
    }

    // > the result of the encryption could be larger than 64 bits so
    //   don't try to handle with an integer
    // > the ``stringd'' ZZ must be reversed because we want the string to go
    //   from high to low order bytes
    // > leading zeros must be added because not all numbers will span the
    //   allotted bytes and we don't want mysql to do a misaligned comparison
    const std::string &enc_string =
        leadingZeros(reverse(StringFromZZ(ope.encrypt(ZZFromUint64(pval)))),
                     this->ciph_size);


    return new Item_string(make_thd_string(enc_string),
                           enc_string.length(),
                           &my_charset_bin);
}

Item *
OPE_int::decrypt(const Item &ctext, uint64_t IV) const
{
    LOG(encl) << "OPE_int decrypt " << ItemToString(ctext) << " IV " << IV
              << std::endl;

    if (MYSQL_TYPE_VARCHAR != this->cinteger.getFieldType()) {
        const ulonglong cval = RiboldMYSQL::val_uint(ctext);
        return new Item_int(static_cast<ulonglong>(uint64FromZZ(ope.decrypt(ZZFromUint64(cval)))));
    }

    // undo the reversal from encryption
    return new Item_int(static_cast<ulonglong>(uint64FromZZ(ope.decrypt(ZZFromString(reverse(ItemToString(ctext)))))));
}


OPE_str::OPE_str(const Create_field &f, const std::string &seed_key)
    : key(prng_expand(seed_key, key_bytes)),
      ope(OPE(key, plain_size * BITS_PER_BYTE, ciph_size * BITS_PER_BYTE))
{}

OPE_str::OPE_str(unsigned int id, const std::string &serial)
    : EncLayer(id), key(serial),
    ope(OPE(key, plain_size * BITS_PER_BYTE, ciph_size * BITS_PER_BYTE))
{}

Create_field *
OPE_str::newCreateField(const Create_field &cf,
                        const std::string &anonname) const
{
    return arrayCreateFieldHelper(cf, cf.length, MYSQL_TYPE_LONGLONG,
                                  anonname, &my_charset_bin);
}

/*
 * Make all characters uppercase as mysql string order comparison
 * is case insensitive.
 *
 * mysql> SELECT 'a' = 'a', 'A' = 'a', 'A' < 'b', 'z' > 'M';
 * +-----------+-----------+-----------+-----------+
 * | 'a' = 'a' | 'A' = 'a' | 'A' < 'b' | 'z' > 'M' |
 * +-----------+-----------+-----------+-----------+
 * |         1 |         1 |         1 |         1 |
 * +-----------+-----------+-----------+-----------+
 */
Item *
OPE_str::encrypt(const Item &ptext, uint64_t IV) const
{
    std::string ps = toUpperCase(ItemToString(ptext));
    if (ps.size() < plain_size)
        ps = ps + std::string(plain_size - ps.size(), 0);

    uint32_t pv = 0;

    for (uint i = 0; i < plain_size; i++) {
        pv = pv * 256 + static_cast<int>(ps[i]);
    }

    const ZZ enc = ope.encrypt(to_ZZ(pv));

    return new (current_thd->mem_root)
               Item_int(static_cast<ulonglong>(uint64FromZZ(enc)));
}

Item *
OPE_str::decrypt(const Item &ctext, uint64_t IV) const
{
    thrower() << "cannot decrypt string from OPE";
}


/**************** HOMFactory ***************************/

std::unique_ptr<EncLayer>
HOMFactory::create(const Create_field &cf, const std::string &key)
{
    if (cf.sql_type == MYSQL_TYPE_DECIMAL
        || cf.sql_type == MYSQL_TYPE_NEWDECIMAL) {
        FAIL_TextMessageError("decimal support is broken");
    }

    return std::unique_ptr<EncLayer>(new HOM(cf, key));
}

std::unique_ptr<EncLayer>
HOMFactory::deserialize(unsigned int id, const SerialLayer &serial) {
    if (serial.name == "HOM_dec") {
        FAIL_TextMessageError("decimal support broken");
    }
    return std::unique_ptr<EncLayer>(new HOM(id, serial.layer_info));
}


/**************************************************************************
****************************ASHEFactory************************************
***************************************************************************
*/


//std::unique_ptr<EncLayer>
//ASHEFactory::create(const Create_field &cf, const std::string &key)
//{
//    if (cf.sql_type == MYSQL_TYPE_DECIMAL
//        || cf.sql_type == MYSQL_TYPE_NEWDECIMAL) {
//        FAIL_TextMessageError("decimal support is broken");
//    }
//
//    return std::unique_ptr<EncLayer>(new ASHE(cf, key));
//}
//
//std::unique_ptr<EncLayer>
//ASHEFactory::deserialize(unsigned int id, const SerialLayer &serial) {
//    if (serial.name == "ASHE_dec") {
//        FAIL_TextMessageError("decimal support broken");
//    }
//    return std::unique_ptr<EncLayer>(new ASHE(id, serial.layer_info));
//}
//


/****************************************************************************
*******************************HOM*******************************************
*****************************************************************************
*/
HOM::HOM(const Create_field &f, const std::string &seed_key)
    : seed_key(seed_key), sk(NULL), waiting(true)
{}

HOM::HOM(unsigned int id, const std::string &serial)
    : EncLayer(id), seed_key(serial), sk(NULL), waiting(true)
{}

Create_field *
HOM::newCreateField(const Create_field &cf,
                    const std::string &anonname) const{
    return arrayCreateFieldHelper(cf, 2*nbits/BITS_PER_BYTE,
                                  MYSQL_TYPE_VARCHAR, anonname,
                                  &my_charset_bin);
}

//if first, use seed key to generate 

void
HOM::unwait() const {
    const std::unique_ptr<streamrng<arc4>>
        prng(new streamrng<arc4>(seed_key));
    sk = new Paillier_priv(Paillier_priv::keygen(prng.get(), nbits));
    waiting = false;
}

Item *
HOM::encrypt(const Item &ptext, uint64_t IV) const{
    if (true == waiting) {
        this->unwait();
    }

    const ZZ enc = sk->encrypt(ItemIntToZZ(ptext));
    return ZZToItemStr(enc);
}

Item *
HOM::decrypt(const Item &ctext, uint64_t IV) const
{
    if (true == waiting) {
        this->unwait();
    }

    const ZZ enc = ItemStrToZZ(ctext);
    const ZZ dec = sk->decrypt(enc);
    LOG(encl) << "HOM ciph " << enc << "---->" << dec;
    TEST_Text(NumBytes(dec) <= 8,
              "Summation produced an integer larger than 64 bits");
    return ZZToItemInt(dec);
}

static udf_func u_sum_a = {
    LEXSTRING("cryptdb_agg"),
    STRING_RESULT,
    UDFTYPE_AGGREGATE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};

static udf_func u_sum_f = {
    LEXSTRING("cryptdb_func_add_set"),
    STRING_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};

Item *
HOM::sumUDA(Item *const expr) const
{
    if (true == waiting) {
        this->unwait();
    }

    List<Item> l;
    l.push_back(expr);
    l.push_back(ZZToItemStr(sk->hompubkey()));
    return new (current_thd->mem_root) Item_func_udf_str(&u_sum_a, l);
}

Item *
HOM::sumUDF(Item *const i1, Item *const i2) const
{
    if (true == waiting) {
        this->unwait();
    }

    List<Item> l;
    l.push_back(i1);
    l.push_back(i2);
    l.push_back(ZZToItemStr(sk->hompubkey()));

    return new (current_thd->mem_root) Item_func_udf_str(&u_sum_f, l);
}

HOM::~HOM() {
    delete sk;
}




/******* SEARCH **************************/

Search::Search(const Create_field &f, const std::string &seed_key)
    : key(prng_expand(seed_key, key_bytes))
{}

Search::Search(unsigned int id, const std::string &serial)
    : EncLayer(id), key(prng_expand(serial, key_bytes))
{}

Create_field *
Search::newCreateField(const Create_field &cf,
                       const std::string &anonname) const
{
    return arrayCreateFieldHelper(cf, cf.length, MYSQL_TYPE_BLOB,
                                  anonname, &my_charset_bin);
}


//returns the concatenation of all words in the given list
static std::string
assembleWords(std::list<std::string> * const words)
{
    std::string res = "";

    for (std::list<std::string>::iterator it = words->begin();
         it != words->end();
         it++) {
        res = res + *it;
    }

    return res;
}

static std::string
encryptSWP(const std::string &key, const std::list<std::string> &words)
{
    const std::unique_ptr<std::list<std::string>>
        l(SWP::encrypt(key, words));
    const std::string r = assembleWords(l.get());
    return r;
}

static Token
token(const std::string &key, const std::string &word)
{
    return SWP::token(key, word);
}

//this function should in fact be provided by the programmer
//currently, we split by whitespaces
// only consider words at least 3 chars in len
// discard not unique objects
static std::list<std::string> *
tokenize(const std::string &text)
{
    std::list<std::string> tokens = split(text, " ,;:.");

    std::set<std::string> search_tokens;

    std::list<std::string> * const res = new std::list<std::string>();

    for (std::list<std::string>::iterator it = tokens.begin();
            it != tokens.end();
            it++) {
        if ((it->length() >= 3) &&
                (search_tokens.find(*it) == search_tokens.end())) {
            const std::string token = toLowerCase(*it);
            search_tokens.insert(token);
            res->push_back(token);
        }
    }

    search_tokens.clear();
    return res;

}

static char *
newmem(const std::string &a)
{
    const unsigned int len = a.length();
    char * const res = new char[len];
    memcpy(res, a.c_str(), len);
    return res;
}

Item *
Search::encrypt(const Item &ptext, uint64_t IV) const
{
    const std::string plainstr = ItemToString(ptext);
    //TODO: remove string, string serves this purpose now..
    const std::list<std::string> * const tokens = tokenize(plainstr);
    const std::string ciph = encryptSWP(key, *tokens);

    LOG(encl) << "SEARCH encrypt " << plainstr << " --> " << ciph;

    return new Item_string(newmem(ciph), ciph.length(), &my_charset_bin);
}

Item *
Search::decrypt(const Item &ctext, uint64_t IV) const
{
    thrower() << "decryption from SWP not supported \n";
}

static udf_func u_search = {
    LEXSTRING("cryptdb_searchSWP"),
    INT_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};


static std::string
searchstrip(std::string s)
{
    if (s[0] == '%') {
        s = s.substr(1, s.length() - 1);
    }
    const uint len = s.length();
    if (s[len-1] == '%') {
        s = s.substr(0, len-1);
    }

    return s;
}

Item *
Search::searchUDF(Item * const field, Item * const expr) const
{
    List<Item> l = List<Item>();

    l.push_back(field);

    // Add token
    const Token t =
        token(key, std::string(searchstrip(ItemToString(*expr))));
    Item_string * const t1 =
        new Item_string(newmem(t.ciph), t.ciph.length(),
                        &my_charset_bin);
    t1->name = NULL; //no alias
    l.push_back(t1);

    Item_string * const t2 =
        new Item_string(newmem(t.wordKey), t.wordKey.length(),
                        &my_charset_bin);
    t2->name = NULL;
    l.push_back(t2);

    return new Item_func_udf_int(&u_search, l);
}

Create_field *
PlainText::newCreateField(const Create_field &cf,
                          const std::string &anonname) const
{
    const THD * const thd = current_thd;
    Create_field * const f0 = cf.clone(thd->mem_root);
    if (anonname.size() > 0) {
        f0->field_name = make_thd_string(anonname);
    }

    return f0;
}

Item *
PlainText::encrypt(const Item &ptext, uint64_t IV) const
{
    return dup_item(ptext);
}

Item *
PlainText::decrypt(const Item &ctext, uint64_t IV) const
{
    return dup_item(ctext);
}

Item *
PlainText::decryptUDF(Item * const col, Item * const ivcol) const
{
    FAIL_TextMessageError("No PLAIN decryption UDF");
}

std::string
PlainText::doSerialize() const
{
    return std::string("");
}

static udf_func u_cryptdb_version = {
    LEXSTRING("cryptdb_version"),
    STRING_RESULT,
    UDFTYPE_FUNCTION,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0L,
};

const std::vector<udf_func*> udf_list = {
    &u_decRNDInt,
    &u_decRNDString,
    &u_decDETInt,
    &u_decDETStr,
    &u_sum_f,
    &u_sum_a,
    &u_search,
    &u_cryptdb_version
};


/************************************************ASHE********************************************/

ASHE::ASHE(const Create_field &f, const std::string &seed_key)
    : seed_key(seed_key)
{}

ASHE::ASHE(unsigned int id, const std::string &serial){}

Create_field *
ASHE::newCreateField(const Create_field &cf,
                    const std::string &anonname) const{
    return NULL;
}

//if first, use seed key to generate
Item *
ASHE::encrypt(const Item &ptext, uint64_t IV) const{
    return NULL;
}

Item *
ASHE::decrypt(const Item &ctext, uint64_t IV) const
{
    return NULL;
}

ASHE::~ASHE() {

}




