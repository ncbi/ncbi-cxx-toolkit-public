#ifndef _ACC_VER_CACHE_EXC_HPP_
#define _ACC_VER_CACHE_EXC_HPP_

#include <stdexcept>
#include <sstream>

#include "uv.h"

class EException: public std::runtime_error {
protected:
    const int m_Code;
public:
    EException(const char * const msg, const int code) noexcept :
        runtime_error{msg},
        m_Code{code}
    {}
    int code() const noexcept {
        return m_Code;
    }
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        throw EException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};

class EAccVerException: public EException {
public:
    EAccVerException(const char * const msg, const int code) noexcept : EException(msg, code) {}
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        throw EAccVerException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};

class EUvException: public EException {
public:
    EUvException(const char * const msg, const int code) noexcept : EException(msg, code) {}
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        if (code) {
            std::stringstream ss;
            ss << msg << ": " << uv_strerror(code);
            throw EUvException(ss.str().c_str(), code);
        }
        else
            throw EUvException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};


#endif
