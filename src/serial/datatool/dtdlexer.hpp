#ifndef DTDLEXER_HPP
#define DTDLEXER_HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   DTD lexer
*
* ===========================================================================
*/

#include "alexer.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class DTDLexer : public AbstractLexer
{
public:
    DTDLexer(CNcbiIstream& in, const string& name);
    virtual ~DTDLexer(void);

protected:
    virtual TToken LookupToken(void);
    virtual void LookupComments(void);

    bool   ProcessComment(void);
    bool   IsIdentifierSymbol(char c);
    TToken LookupIdentifier(void);
    TToken LookupKeyword(void);
    TToken LookupEntity(void);
    TToken LookupString(void);
    bool   EndPrevToken(void);

    size_t m_CharsToSkip;
private:
    bool m_IdentifierEnd;
};

END_NCBI_SCOPE

#endif // DTDLEXER_HPP
