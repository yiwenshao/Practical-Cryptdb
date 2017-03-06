#include <util/util.hh>

#include <string>

#include <sql_parse.h>

#include <parser/mysql_type_metadata.hh>
#include <parser/sql_utils.hh>

#include <crypto/BasicCrypto.hh>

std::pair<enum enum_field_types, unsigned long>
genericVarCharCrypto(unsigned long len, bool pad);

// ########################################
// ########################################
//             integer types
// ########################################
// ########################################
// ::isRangeSupported(...) assumes fields are unsigned

template <enum enum_field_types id>
Item *
MySQLIntegerMetaData<id>::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_int(static_cast<long long>(valFromStr(value)));
}

static uint64_t
inclusiveUpperBound(unsigned bits)
{
    if (64 == bits) {
        return UINT64_MAX;
    }

    return (1LL << bits) - 1;
}

static std::pair<int64_t, uint64_t>
supportsRangeHelper(const Create_field &field, unsigned bytes)
{
    assert(bytes <= 8);

    const unsigned int bits = 8 * bytes;
    const signage s = (field.flags & UNSIGNED_FLAG) == UNSIGNED_FLAG
        ? signage::UNSIGNED : signage::SIGNED;
    AssignOnce<bool> status;
    if (signage::UNSIGNED == s) {
        return std::make_pair(0, inclusiveUpperBound(bits));
    }

    assert(signage::SIGNED == s);
    const uint64_t edge = inclusiveUpperBound(bits - 1);
    return std::make_pair(-static_cast<int64_t>(edge + 1), edge);
}

std::pair<int64_t, uint64_t>
MySQLTinyMetaData::supportsRange(const Create_field &field) const
{
    // 1 byte
    const unsigned int bytes = 1;
    return supportsRangeHelper(field, bytes);
}

bool
MySQLTinyMetaData::isRangeSupported(std::pair<int64_t, uint64_t> inclusiveRange) const
{
    // signed   : -128  127
    // unsigned : 0     255
    const int64_t minimum  = 0;
    const uint64_t maximum = 255;
    return inclusiveRange.first >= minimum
        && inclusiveRange.second <= maximum;
}

std::pair<int64_t, uint64_t>
MySQLShortMetaData::supportsRange(const Create_field &field) const
{
    // 2 bytes
    const unsigned int bytes = 2;
    return supportsRangeHelper(field, bytes);
}

bool
MySQLShortMetaData::isRangeSupported(std::pair<int64_t, uint64_t> inclusiveRange) const
{
    // signed   : -32768    32767
    // unsigned : 0         65535
    const int64_t minimum  = 0;
    const uint64_t maximum = 65535;
    return inclusiveRange.first >= minimum
        && inclusiveRange.second <= maximum;
}

std::pair<int64_t, uint64_t>
MySQLInt24MetaData::supportsRange(const Create_field &field) const
{
    // 3 bytes
    const unsigned int bytes = 3;
    return supportsRangeHelper(field, bytes);
}

bool
MySQLInt24MetaData::isRangeSupported(std::pair<int64_t, uint64_t> inclusiveRange) const
{
    // signed   : -8388608  8388607
    // unsigned : 0         16777215
    const int64_t minimum  = 0;
    const uint64_t maximum = 16777215L;
    return inclusiveRange.first >= minimum
        && inclusiveRange.second <= maximum;
}

std::pair<int64_t, uint64_t>
MySQLLongMetaData::supportsRange(const Create_field &field) const
{
    // 4 bytes
    const unsigned int bytes = 4;
    return supportsRangeHelper(field, bytes);
}

bool
MySQLLongMetaData::isRangeSupported(std::pair<int64_t, uint64_t> inclusiveRange) const
{
    // signed   : -2147483648   2147483647
    // unsigned : 0             4294967295
    const int64_t minimum  = 0;
    const uint64_t maximum = 4294967295L;
    return inclusiveRange.first >= minimum
        && inclusiveRange.second <= maximum;
}

std::pair<int64_t, uint64_t>
MySQLLongLongMetaData::supportsRange(const Create_field &field) const
{
    // 8 bytes
    const unsigned int bytes = 8;
    return supportsRangeHelper(field, bytes);
}

bool
MySQLLongLongMetaData::isRangeSupported(std::pair<int64_t, uint64_t> inclusiveRange) const
{
    // signed   : -9223372036854775808      9223372036854775807
    // unsigned : 0                         18446744073709551615
    const int64_t minimum  = 0;
    const uint64_t maximum = 18446744073709551615ULL;
    return inclusiveRange.first >= minimum
        && inclusiveRange.second <= maximum;
}

#undef RANGE_TEST

// ########################################
// ########################################
//          fixed-point types
// ########################################
// ########################################
template <enum enum_field_types id>
Item *
AbstractMySQLDecimalMetaData<id>::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_string(make_thd_string(value), value.length(),
                    &my_charset_bin);
}

// ########################################
// ########################################
//              float types
// ########################################
// ########################################
template <enum enum_field_types id>
Item *
AbstractMySQLFloatMetaData<id>::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_float(value.c_str(), value.size());
}

/*
    For arguments that have no fixed number of decimals, the
    `decimals' value is set to 31, which is 1 more than the maximum
    number of decimals permitted for the `DECIMAL':
    numeric-types,  `FLOAT': numeric-types, and `DOUBLE':
    numeric-types. data types. As of MySQL 5.5.3, this value is
    available as the constant `NOT_FIXED_DEC' in the `mysql_com.h'
    header file.

    in short; 'real' is encoded as an invalid floating point type.
*/

const std::string
MySQLFloatMetaData::humanReadable(const Create_field &f) const
{
    if (isRealEncoded(f)) {
        return "REAL";
    }

    return "FLOAT";
}

const std::string
MySQLDoubleMetaData::humanReadable(const Create_field &f) const
{
    if (isRealEncoded(f)) {
        return "REAL";
    }

    return "DOUBLE";
}

bool isRealEncoded(const Create_field &f)
{
    switch (f.sql_type) {
        case MYSQL_TYPE_FLOAT:
            return NOT_FIXED_DEC == f.decimals &&
                ((FLT_DIG + 6) == f.length
                 || MAX_FLOAT_STR_LENGTH == f.length);
        case MYSQL_TYPE_DOUBLE:
            return NOT_FIXED_DEC == f.decimals &&
                ((DBL_DIG + 6) == f.length
                 || MAX_DOUBLE_STR_LENGTH == f.length);
        default:
            return false;
    }
}

// ########################################
// ########################################
//             string(*) types
// ########################################
// ########################################
template <enum enum_field_types id>
std::pair<enum enum_field_types, unsigned long>
AbstractMySQLStringMetaData<id>::AESTypeAndLength(unsigned long len,
                                                  bool pad) const
{
    return genericVarCharCrypto(len, pad);
}

template <enum enum_field_types id>
Item *
AbstractMySQLStringMetaData<id>::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_string(make_thd_string(value), value.length(),
                    &my_charset_bin);
}

const std::string
MySQLVarCharMetaData::humanReadable(const Create_field &f) const
{
    if (f.charset == &my_charset_bin) {
        return "VARBINARY";
    } else {
        return "VARCHAR";
    }
}

const std::string
MySQLStringMetaData::humanReadable(const Create_field &f) const
{
    if (f.charset == &my_charset_bin) {
        return "BINARY";
    } else {
        return "CHAR";
    }
}

// ########################################
// ########################################
//                date types
// ########################################
// ########################################
template <enum enum_field_types id>
std::pair<enum enum_field_types, unsigned long>
AbstractMySQLDateMetaData<id>::AESTypeAndLength(unsigned long len,
                                                 bool pad) const
{
    return genericVarCharCrypto(len, pad);
}

template <enum enum_field_types id>
Item *
AbstractMySQLDateMetaData<id>::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_string(make_thd_string(value), value.length(),
                    &my_charset_bin);
}


// ########################################
// ########################################
//               enum types
// ########################################
// ########################################
Item *
MySQLEnumMetaData::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_string(make_thd_string(value), value.length(),
                    &my_charset_bin);
}


// ########################################
// ########################################
//               blob types
// ########################################
// ########################################
template <enum enum_field_types id>
std::pair<enum enum_field_types, unsigned long>
AbstractMySQLBlobMetaData<id>::AESTypeAndLength(unsigned long len,
                                                bool pad) const
{
    // HACK: add proper support for BLOB
    if (MYSQL_TYPE_BLOB == id && len % AES_BLOCK_BYTES != 0) {
        thrower() << "cryptdb does not support BLOB";
    }
    return std::make_pair(id, len);
}

template <enum enum_field_types id>
Item *
AbstractMySQLBlobMetaData<id>::intoItem(const std::string &value) const
{
    return new (current_thd->mem_root)
        Item_string(make_thd_string(value), value.length(),
                    &my_charset_bin);
}

const std::string
MySQLTinyBlobMetaData::humanReadable(const Create_field &f) const
{
    if (f.charset == &my_charset_bin) {
        return "TINYBLOB";
    } else {
        return "TINYTEXT";
    }
}

const std::string
MySQLMediumBlobMetaData::humanReadable(const Create_field &f) const
{
    if (f.charset == &my_charset_bin) {
        return "MEDIUMBLOB";
    } else {
        return "MEDIUMTEXT";
    }
}
const std::string
MySQLBlobMetaData::humanReadable(const Create_field &f) const
{
    if (f.charset == &my_charset_bin) {
        return "BLOB";
    } else {
        return "TEXT";
    }
}

const std::string
MySQLLongBlobMetaData::humanReadable(const Create_field &f) const
{
    if (f.charset == &my_charset_bin) {
        return "LONGBLOB";
    } else {
        return "LONGTEXT";
    }
}

std::map<enum enum_field_types,
         std::unique_ptr<AbstractMySQLTypeMetaData> >
buildMySQLTypeMetaData()
{
    std::map<enum enum_field_types,
             std::unique_ptr<AbstractMySQLTypeMetaData> > m;
    m[MYSQL_TYPE_TINY] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLTinyMetaData);
    m[MYSQL_TYPE_SHORT] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLShortMetaData);
    m[MYSQL_TYPE_INT24] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLInt24MetaData);
    m[MYSQL_TYPE_LONG] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLLongMetaData);
    m[MYSQL_TYPE_LONGLONG] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLLongLongMetaData);
    m[MYSQL_TYPE_DECIMAL] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLDecimalMetaData);
    m[MYSQL_TYPE_NEWDECIMAL] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLNewDecimalMetaData);
    m[MYSQL_TYPE_FLOAT] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLFloatMetaData);
    m[MYSQL_TYPE_DOUBLE] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLDoubleMetaData);
    m[MYSQL_TYPE_VARCHAR] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLVarCharMetaData);
    m[MYSQL_TYPE_VAR_STRING] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLVarStringMetaData);
    m[MYSQL_TYPE_STRING] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLStringMetaData);
    m[MYSQL_TYPE_DATE] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLDateMetaData);
    m[MYSQL_TYPE_NEWDATE] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLNewDateMetaData);
    m[MYSQL_TYPE_TIME] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLTimeMetaData);
    m[MYSQL_TYPE_DATETIME] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLDateTimeMetaData);
    m[MYSQL_TYPE_TIMESTAMP] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLTimeStampMetaData);
    m[MYSQL_TYPE_ENUM] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLEnumMetaData);
    m[MYSQL_TYPE_TINY_BLOB] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLTinyBlobMetaData);
    m[MYSQL_TYPE_MEDIUM_BLOB] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLMediumBlobMetaData);
    m[MYSQL_TYPE_BLOB] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLBlobMetaData);
    m[MYSQL_TYPE_LONG_BLOB] =
        std::unique_ptr<AbstractMySQLTypeMetaData>(new MySQLLongBlobMetaData);

    return m;
}

const std::map<enum enum_field_types,
               std::unique_ptr<AbstractMySQLTypeMetaData> >
    mysql_meta_data = buildMySQLTypeMetaData();

static const
std::unique_ptr<AbstractMySQLTypeMetaData> &
fetch(enum enum_field_types type)
{
    const auto it = mysql_meta_data.find(type);
    if (mysql_meta_data.end() == it) {
        thrower() << "unsupported MYSQL field type " << type << std::endl;
    }

    return it->second;
}

static const AbstractMySQLIntegerMetaData &
fetchInt(enum enum_field_types type)
{
    const std::unique_ptr<AbstractMySQLTypeMetaData> &abstract =
        fetch(type);
    if (!abstract->isNumeric()) {
        thrower() << "bad type to supportsRange";
    }

    // bad things are about to happen if you tried to fetch a numeric
    // non integer type (ie decimal)
    const AbstractMySQLIntegerMetaData *const integer_meta =
        static_cast<AbstractMySQLIntegerMetaData *>(abstract.get());
    return *integer_meta;
}

const std::string
MySQLTypeToText(const Create_field &f)
{
    return fetch(f.sql_type)->humanReadable(f);
}

bool
encryptionSupported(const Create_field &f)
{
    return fetch(f.sql_type)->encryptionSupported();
}

bool
isMySQLTypeNumeric(enum enum_field_types type)
{
    return fetch(type)->isNumeric();
}

bool
isMySQLTypeNumeric(const Create_field &f)
{
    return isMySQLTypeNumeric(f.sql_type);
}

std::pair<int64_t, uint64_t>
supportsRange(const Create_field &f)
{
    return fetchInt(f.sql_type).supportsRange(f);
}

std::pair<bool, enum enum_field_types>
getTypeForRange(std::pair<int64_t, uint64_t> inclusiveRange)
{
    // FIXME: find easier to maintain way to do this
    // > list must be in order from small to large
    static std::vector<enum enum_field_types>
        integer_types({MYSQL_TYPE_TINY, MYSQL_TYPE_SHORT, MYSQL_TYPE_INT24,
                       MYSQL_TYPE_LONG, MYSQL_TYPE_LONGLONG});
    for (auto it : integer_types) {
        const AbstractMySQLIntegerMetaData &integer_meta =
            fetchInt(it);
        if (integer_meta.isRangeSupported(inclusiveRange)) {
            return std::make_pair(true, it);
        }
    }

    return std::make_pair(false, static_cast<enum enum_field_types>(-1));
}

Item *
MySQLFieldTypeToItem(enum enum_field_types type, const std::string &value)
{
    return fetch(type)->intoItem(value);
}

std::pair<enum enum_field_types, unsigned long>
AESTypeAndLength(const Create_field &f, bool pad)
{
    return fetch(f.sql_type)->AESTypeAndLength(f.length, pad);
}

std::pair<enum enum_field_types, unsigned long>
genericVarCharCrypto(unsigned long len, bool pad)
{
    unsigned long out_len;
    if (false == rounded_len(len, AES_BLOCK_BYTES, pad, &out_len)) {
        thrower() << "rounded_len failed, you may be trying to create too"
                     " large of a field";
    }

    return std::make_pair(MYSQL_TYPE_VARCHAR, out_len);
}

