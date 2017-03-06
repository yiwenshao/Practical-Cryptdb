#pragma once

#include <main/rewrite_ds.hh>
#include <main/error.hh>

#define TYPENAME(value)                                         \
    std::string typeName() const {return (value);}              \
    static std::string instanceTypeName() {return (value);}


#define RETURN_NULL_IF_NULL(p)              \
    (p);                                    \
    if (!(p)) {                             \
        return NULL;                        \
    }

#define ROLLBACK_AND_RFIF(status, conn)                 \
{                                                       \
    if (!(status)) {                                    \
        assert((conn)->execute("ROLLBACK;"));           \
        return false;                                   \
    }                                                   \
}                                                       \

inline void
testBadItemArgumentCount(const std::string &file_name,
                         unsigned int line_number, int type, int expected,
                         int actual)
{
    if (expected != actual) {
        throw BadItemArgumentCount(file_name, line_number, type, expected,
                                   actual);
    }
}

#define TEST_BadItemArgumentCount(type, expected, actual)       \
{                                                               \
    testBadItemArgumentCount(__FILE__, __LINE__, (type),        \
                             (expected), (actual));             \
}

inline void
testUnexpectedSecurityLevel(const std::string &file_name,
                            unsigned int line_number, onion o,
                            SECLEVEL expected, SECLEVEL actual)
{
    if (expected != actual) {
        throw UnexpectedSecurityLevel(file_name, line_number, o,
                                      expected, actual);
    }
}

#define TEST_UnexpectedSecurityLevel(o, expected, actual)           \
{                                                                   \
    testUnexpectedSecurityLevel(__FILE__, __LINE__, (o),            \
                                (expected), (actual));              \
}

// FIXME: Nicer to take reason instead of 'why'.
inline void
testNoAvailableEncSet(const std::string &file_name,
                      unsigned int line_number, const EncSet &enc_set,
                      unsigned int type, const EncSet &req_enc_set,
                      const std::string &why,
                      const std::vector<std::shared_ptr<RewritePlan> >
                        &childr_rp)
{
    if (false == enc_set.available()) {
        throw NoAvailableEncSet(file_name, line_number, type,
                                req_enc_set, why, childr_rp);
    }
}

#define TEST_NoAvailableEncSet(enc_set, type, req_enc_set, why,     \
                               childr_rp)                           \
{                                                                   \
    testNoAvailableEncSet(__FILE__, __LINE__, (enc_set), (type),    \
                          (req_enc_set), (why), (childr_rp));       \
}

inline void
testTextMessageError(const std::string &file_name,
                     unsigned int line_number, bool test,
                     const std::string &message)
{
    if (false == test) {
        throw TextMessageError(file_name, line_number, message);
    }
}

#define TEST_TextMessageError(test, message)                        \
{                                                                   \
    testTextMessageError(__FILE__, __LINE__, (test), (message));    \
}                                                                   \

#define TEST_Text TEST_TextMessageError

#define FAIL_TextMessageError(message)                              \
{                                                                   \
    throw TextMessageError(__FILE__, __LINE__, (message));          \
}

#define TEST_DatabaseDiscrepancy(db_a, db_b)                        \
{                                                                   \
    TEST_TextMessageError((db_a) == (db_b),                         \
                          "Database discrepancy!");                 \
}

inline void
testIdentifierNotFound(const std::string &file_name,
                       unsigned int line_number, bool test,
                       const std::string &identifier_name)
{
    if (false == test) {
        throw IdentifierNotFound(file_name, line_number,
                                 identifier_name);
    }
}

#define TEST_IdentifierNotFound(test, identifier_name)              \
{                                                                   \
    testIdentifierNotFound(__FILE__, __LINE__, (test),              \
                          (identifier_name));                       \
}

inline void
testDatabaseNotFound(const std::string &file_name,
                       unsigned int line_number, bool test,
                       const std::string &identifier_name)
{
    const std::string msg =
        identifier_name.size() == 0 ? "must select a database"
                                    : "database '" + identifier_name +
                                      "' not found";
    if (false == test) {
        throw TextMessageError(file_name, line_number,
                               msg);
    }
}
#define TEST_DatabaseNotFound(test, identifier_name)                \
{                                                                   \
    testDatabaseNotFound(__FILE__, __LINE__, (test),                \
                          (identifier_name));                       \
}

#define SYNC_IF_FALSE(status, conn)                     \
{                                                       \
    const auto &s = (status);                            \
    if (!s) {                                           \
        assert((conn)->execute("ROLLBACK;"));           \
        TEST_ErrPkt(s, "query failed");                 \
    }                                                   \
}

#define CR_ROLLBACK_AND_FAIL(res, msg)                                  \
{                                                                       \
    if (false == res.success()) {                                       \
        yield return CR_QUERY_AGAIN("ROLLBACK");                        \
                                                                        \
        assert(res.success());                                          \
        FAIL_GenericPacketException((msg));                             \
    }                                                                   \
}

#define ROLLBACK_ERROR_PACKET                                               \
{                                                                           \
    throw ErrorPacketException(__FILE__, __LINE__, "proxy did rollback",    \
                               1213, "40001");                              \
}
