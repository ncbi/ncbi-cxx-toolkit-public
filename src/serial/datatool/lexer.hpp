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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  1999/11/15 19:36:16  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

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
