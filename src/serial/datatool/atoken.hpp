#ifndef ABSTRACT_TOKEN_HPP
#define ABSTRACT_TOKEN_HPP

#include <corelib/ncbistd.hpp>

USING_NCBI_SCOPE;

typedef int TToken;
static const TToken T_SYMBOL = 0;
static const TToken T_EOF = -1;

class AbstractToken {
public:

    TToken GetToken(void) const
        {
            return token;
        }

    string GetText(void) const
        {
            return string(start, length);
        }

    char GetSymbol(void) const
        {
            return *start;
        }

    int GetLine(void) const
        {
            return line;
        }

    TToken token;
    const char* start;
    int length;
    int line;
};

#endif
