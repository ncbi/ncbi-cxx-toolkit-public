#ifndef ASNLEXER_HPP
#define ASNLEXER_HPP

#include "alexer.hpp"

class ASNLexer : public AbstractLexer {
public:
    ASNLexer(CNcbiIstream& in);
    virtual ~ASNLexer();

protected:
    TToken LookupToken(void);

    void SkipComment(void);
    void LookupNumber(void);
    void LookupIdentifier(void);
    void LookupString(void);
    TToken LookupBinHexString(void);
    TToken LookupKeyword(void);
};

#endif
