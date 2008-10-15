#ifndef OLIGOFAR_DEBUG__HPP
#define OLIGOFAR_DEBUG__HPP

#include <sstream>
#include <stdexcept>

#define DISPLAY(a) "\t"#a" = [" << a << "]"

#define ASSERT(a)                                                       \
    do {                                                                \
        if( !(a) ) {                                                    \
            THROW( std::logic_error, "Assertion failed: "#a" in file "__FILE__" line " << __LINE__ ); \
        }                                                               \
    } while(0)

#define THROW(x,m)                                                      \
    do {                                                                \
        std::ostringstream o;                                           \
        if( isatty(2) ) o << "\x1b[31m";                                \
        o << "\n" << CStackTrace(#x) << "\n";                           \
        if( isatty(2) ) o << "\x1b[32m";                                \
        o << "\nIn file "__FILE__" line " << __LINE__;                  \
        if( isatty(2) ) o << "\x1b[33m";                                \
        o << " " << #x;                                                 \
        if( isatty(2) ) o << "\x1b[34m";                                \
        o << ": ";                                                      \
        if( isatty(2) ) o << "\x1b[35m";                                \
        o << m;                                                         \
        if( isatty(2) ) o << "\x1b[0m";                                 \
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

#endif
