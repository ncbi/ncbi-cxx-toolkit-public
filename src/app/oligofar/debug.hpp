#ifndef OLIGOFAR_DEBUG__HPP
#define OLIGOFAR_DEBUG__HPP

#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>

#define DISPLAY(a) "\t"#a" = [" << a << "]"

#ifndef NOASSERT
#define ASSERT(a)                                                       \
    do {                                                                \
        if( !(a) ) {                                                    \
            THROW( std::logic_error, "Assertion failed: "#a" in file "__FILE__" line " << __LINE__ ); \
        }                                                               \
    } while(0)
#else
#define ASSERT(a)
#endif

#ifdef _WIN32
#define TTYATTR(a) ""
#else
#define TTYATTR(a) "\x1b[" #a 
#endif

#define THROW(x,m)                                                      \
    do {                                                                \
        std::ostringstream o;                                           \
        o << "\n" << TTYATTR(31m) << CStackTrace(#x) << "\n"            \
          << "\n" << TTYATTR(32m) << "In file "                         \
          << TTYATTR(33m) << __FILE__ << TTYATTR(32m) << " line "       \
          << TTYATTR(33m) << __LINE__ << TTYATTR(34m) << " " << #x      \
          << TTYATTR(35m) << ": " << TTYATTR(36m) << m << TTYATTR(0m)   \
          << "\n";                                                      \
        throw x(o.str());                                               \
    } while(0)

#define TESTVAL(type,expr,val)                                          \
    do {                                                                \
        type a_ = expr;                                                 \
        type b_ = val;                                                  \
        if( a_ != b_ ) {                                                \
            std::ostringstream o;                                       \
            o << "Assertion " << #expr << " == " << #val << " failed:\n"; \
            o << " : " << (a_) << " (" #expr ")\n";                      \
            o << " : " << (b_) << " (" #val ")\n";                      \
            string x_ = o.str();                                        \
            THROW( logic_error, x_ );                                   \
        }                                                               \
    }                                                                   \
    while(0)

class __Enter__ 
{ 
public: 
    __Enter__( const std::string& fun ) : m_fun( fun ) { std::cerr << "\x1b[31mSTART\t\x1b[33m" << m_fun << "\x1b[0m\n"; } 
    ~__Enter__() { std::cerr << "\x1b[32mDONE\t\x1b[33m" << m_fun << "\x1b[0m\n"; } 
protected: 
    std::string m_fun; 
}; 

#define ENTER  __Enter__ __enter__( __PRETTY_FUNCTION__ )

#endif
