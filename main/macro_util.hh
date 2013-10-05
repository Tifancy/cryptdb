#pragma once

#include <main/rewrite_ds.hh>
#include <main/error.hh>

#define RETURN_FALSE_IF_FALSE(status)       \
{                                           \
    if (!(status)) {                        \
        return false;                       \
    }                                       \
}

#define RFIF RETURN_FALSE_IF_FALSE

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

#define FAIL_TextMessageError(message)                              \
{                                                                   \
    throw TextMessageError(__FILE__, __LINE__, (message));          \
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

#define TEST_IdentifierNotFound(test, identifier_name)                 \
{                                                                      \
    testIdentifierNotFound(__FILE__, __LINE__, (test),                 \
                          (identifier_name));                          \
}
