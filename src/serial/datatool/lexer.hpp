#ifndef ASNLEXER_HPP
#define ASNLEXER_HPP

/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Eugene Vasilchenko
*
* File Description:
*   ASN.1 lexer
*
*/

#include "alexer.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class ASNLexer : public AbstractLexer
{
    typedef AbstractLexer CParent;
public:
    ASNLexer(CNcbiIstream& in, const string& name);
    virtual ~ASNLexer();

    const string& StringValue(void) const
        {
            return m_StringValue;
        }

    bool AllowIDsEndingWithMinus(void) const
        {
            return m_AllowIDsEndingWithMinus;
        }
    void AllowIDsEndingWithMinus(bool allow)
        {
            m_AllowIDsEndingWithMinus = allow;
        }

protected:
    TToken LookupToken(void);
    void LookupComments(void);

    void StartString(void);
    void AddStringChar(char c);

    void SkipComment(void);
    TToken LookupNumber(void);
    void LookupIdentifier(void);
    void LookupString(void);
    void LookupTag(void);
    TToken LookupBinHexString(void);
    TToken LookupKeyword(void);

    string m_StringValue;
    bool m_AllowIDsEndingWithMinus;
};

END_NCBI_SCOPE

#endif
