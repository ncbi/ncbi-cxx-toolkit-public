#ifndef __DDRPC_HPP__
#define __DDRPC_HPP__

#include <string>
#include <stdexcept>
#include <limits>

namespace DDRPC {
    
constexpr const long INDEFINITE = std::numeric_limits<long>::max();
constexpr const char* CONTENT_TYPE = "application/x-ddrpc";

template <typename T, std::size_t N>
constexpr std::size_t countof(T const (&)[N]) noexcept {
    return N;
};

class EDdRpcException: public std::runtime_error {
protected:
    const int m_Code;
public:
    EDdRpcException(const char * const msg, const int code) noexcept : 
        runtime_error{msg},
        m_Code{code}
    {}
    [[noreturn]] static inline void raise(const char* msg, int code = 0) {
        throw EDdRpcException{msg, code};
    }
    [[noreturn]] static inline void raise(const std::string& msg, int code = 0) {
        raise(msg.c_str(), code);
    }
};


};

#endif
