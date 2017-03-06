#pragma once

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

class CryptDBError {
public:
    CryptDBError(const std::string &m) : msg(m) {}
    std::string msg;
};

class CryptoError : public CryptDBError {
public:
    CryptoError(const std::string &m) : CryptDBError(m) {}
};

inline void
throw_c(bool test, const std::string &msg = "crypto fail")
{
    if (false == test) {
        throw CryptoError(msg);
    }

    return;
}

class err_stream {
 public:
    virtual ~err_stream() noexcept(false) {}

    template <typename T>
    std::ostream &operator<<(T &s) {
        stream << s;
        return stream;
    }

 protected:
    std::stringstream stream;
};


class fatal : public err_stream {
 public:
    ~fatal() noexcept(false) __attribute__((noreturn)) {
        std::cerr << stream.str() << std::endl;
        exit(-1);
    }
};

class thrower : public err_stream {
 public:
    ~thrower() noexcept(false) __attribute__((noreturn)) {
        throw CryptDBError(stream.str());
    }
};

