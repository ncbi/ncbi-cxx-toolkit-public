#ifndef ABSTRACT_TOKEN_HPP
#define ABSTRACT_TOKEN_HPP

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
*   Abstract lexer token
*
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

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
            return start ? string(start, length) : kEmptyStr;
        }

    char GetSymbol(void) const
        {
            return start ? *start : 0;
        }

    int GetLine(void) const
        {
            return line;
        }

    TToken token;
    const char* start;
    size_t length;
    int line;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2006/10/18 12:59:07  gouriano
* Moved Log to bottom
*
* Revision 1.4  2006/05/03 14:37:38  gouriano
* Added parsing attribute definition and include
*
* Revision 1.3  2000/11/20 17:26:10  vasilche
* Fixed warnings on 64 bit platforms.
*
* Revision 1.2  2000/04/07 19:26:06  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:13  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.3  1999/11/15 19:36:12  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/
