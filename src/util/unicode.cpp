/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author: Aleksey Vinokurov
 *
 * File Description:
 *    Unicode transformation library
 *
 */

#include <ncbi_pch.hpp>
#include <util/unicode.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(utf8)

#include "unicode_plans/plan00.inc"
#include "unicode_plans/plan01.inc"
#include "unicode_plans/plan02.inc"
#include "unicode_plans/plan03.inc"
#include "unicode_plans/plan04.inc"
#include "unicode_plans/plan1e.inc"
#include "unicode_plans/plan20.inc"
#include "unicode_plans/plan21.inc"
#include "unicode_plans/plan22.inc"
#include "unicode_plans/plan23.inc"
#include "unicode_plans/plan24.inc"
#include "unicode_plans/plan25.inc"
#include "unicode_plans/plan26.inc"
#include "unicode_plans/plan27.inc"
#include "unicode_plans/plan30.inc"
#include "unicode_plans/plane0.inc"
#include "unicode_plans/plane2.inc"
#include "unicode_plans/plane3.inc"
#include "unicode_plans/plane4.inc"
#include "unicode_plans/plane5.inc"
#include "unicode_plans/plane6.inc"
#include "unicode_plans/plane7.inc"
#include "unicode_plans/plane8.inc"
#include "unicode_plans/planea.inc"
#include "unicode_plans/planeb.inc"
#include "unicode_plans/planfb.inc"
#include "unicode_plans/planfe.inc"


static TUnicodeTable g_DefaultUnicodeTable =
{
    &s_Plan_00h, &s_Plan_01h, &s_Plan_02h, &s_Plan_03h, &s_Plan_04h, 0, 0, 0,  // Plan 00 - 07
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 08 - 0F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 10 - 17
    0, 0, 0, 0, 0, 0, &s_Plan_1Eh, 0,  // Plan 18 - 1F

    &s_Plan_20h, &s_Plan_21h, &s_Plan_22h, &s_Plan_23h, &s_Plan_24h, &s_Plan_25h, &s_Plan_26h, &s_Plan_27h,  // Plan 20 - 27
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 28 - 2F

    &s_Plan_30h, 0, 0, 0, 0, 0, 0, 0,  // Plan 30 - 37
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 38 - 3F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 40 - 47
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 48 - 4F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 50 - 57
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 58 - 5F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 60 - 67
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 68 - 6F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 70 - 77
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 78 - 7F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 80 - 87
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 88 - 8F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 90 - 97
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan 98 - 9F

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan A0 - A7
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan A8 - AF

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan B0 - B7
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan B8 - BF

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan C0 - C7
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan C8 - CF

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan D0 - D7
    0, 0, 0, 0, 0, 0, 0, 0,  // Plan D8 - DF

    &s_Plan_E0h, 0, &s_Plan_E2h, &s_Plan_E3h, &s_Plan_E4h, &s_Plan_E5h, &s_Plan_E6h, &s_Plan_E7h,  // Plan E0 - E7
    &s_Plan_E8h, 0, &s_Plan_EAh, &s_Plan_EBh, 0, 0, 0, 0,  // Plan E8 - EF

    0, 0, 0, 0, 0, 0, 0, 0,  // Plan F0 - F7
    0, 0, 0, &s_Plan_FBh, 0, 0, &s_Plan_FEh, 0   // Plan F8 - FF
};


const SUnicodeTranslation*
UnicodeToAscii(TUnicode character, const TUnicodeTable* table)
{
    if (!table) {
        table = &g_DefaultUnicodeTable;
    }
    unsigned int thePlanNo   = (character & 0xFF00) >> 8;
    unsigned int theOffset = character & 0xFF;
    const TUnicodePlan* thePlan = (*table)[thePlanNo];
    if ( !thePlan ) return 0;
    return &((*thePlan)[theOffset]);
}


TUnicode UTF8ToUnicode( const char* theUTF )
{
    int seq_len;
    const char *p = theUTF;
    char counter = *p++;
    unsigned char c;

    if ( ((*theUTF) & 0xC0) != 0xC0 )
    {
        TUnicode RC = 0;
        RC |= (unsigned char)theUTF[0];
        return RC;
    }

    TUnicode acc = counter & 037;

    while((counter <<= 1) < 0) {
        c = *p++;
        if((c & ~077) != 0200) { // Broken UTF-8 chain
            seq_len = int(p - theUTF);
            return ~0;
        }
        acc = (acc << 6) | (c & 077);
    }
    return acc;
}


int UTF8ToUnicode( const char* theUTF, TUnicode* theUnicode )
{
    int seq_len;
    const char *p = theUTF;
    char counter = *p++;
    unsigned char c;

    if ( (unsigned char )theUTF[0] < 0x80 ) {
        // This is one character UTF8. I.e. regular character.
        *theUnicode = *theUTF;
        return 1;
    }

    if ( ((*theUTF) & 0xC0) != 0xC0 ) {
        // This is not a unicode
        return 0;
    }

    TUnicode acc = counter & 037;

    while((counter <<= 1) < 0) {
        c = *p++;
        if((c & ~077) != 0200) { // Broken UTF-8 chain
            seq_len = int(p - theUTF);
            return ~0;
        }
        acc = (acc << 6) | (c & 077);
    } // while
    seq_len = int(p - theUTF);
    *theUnicode = acc;
    return seq_len;
}


string UnicodeToUTF8( TUnicode theUnicode )
{
    char theBuffer[10];
    int theLength = UnicodeToUTF8( theUnicode, theBuffer, 10 );
    return string( theBuffer, theLength );
}


int UnicodeToUTF8( TUnicode theUnicode, char *theBuffer, int theBufLength )
{
    int Length = 0;

    if (theUnicode < 0x80) {
        Length = 1;
        if ( Length > theBufLength ) return 0;
        theBuffer[0] = char(theUnicode);
    }
    else if (theUnicode < 0x800) {
        Length = 2;
        if ( Length > theBufLength ) return 0;
        theBuffer[0] = char(0xC0 | theUnicode>>6);
        theBuffer[1] = char(0x80 | theUnicode & 0x3F);
    }
    else if (theUnicode < 0x10000) {
        Length = 3;
        if ( Length > theBufLength ) return 0;
        theBuffer[0] = char(0xE0 | theUnicode>>12);
        theBuffer[1] = char(0x80 | theUnicode>>6 & 0x3F);
        theBuffer[2] = char(0x80 | theUnicode & 0x3F);
    }
    else if (theUnicode < 0x200000) {
        Length = 4;
        if ( Length > theBufLength ) return 0;
        theBuffer[0] = char(0xF0 | theUnicode>>18);
        theBuffer[1] = char(0x80 | theUnicode>>12 & 0x3F);
        theBuffer[2] = char(0x80 | theUnicode>>6 & 0x3F);
        theBuffer[3] = char(0x80 | theUnicode & 0x3F);
    }
    return Length;
}


int UTF8ToAscii( const char* src, char* dst,
                 int dstLen, const TUnicodeTable* table)
{
    if ( !src || !dst || dstLen == 0 ) return 0;
    size_t srcPos = 0;
    size_t dstPos = 0;
    size_t srcLen = strlen( src );

    for ( srcPos = 0; srcPos < srcLen; )
    {
        // Assign quck pointers
        char* pDst = &(dst[dstPos]);
        const char* pSrc = &(src[srcPos]);
        TUnicode theUnicode;

        int utfLen = UTF8ToUnicode( pSrc, &theUnicode );

        if ( (utfLen == 0) || (utfLen == -1) ) {
            // Skip the error.
            srcPos++;
            continue;
        }

        srcPos += utfLen;

        // Find the correct substitution.
        const SUnicodeTranslation*
            pSubst = UnicodeToAscii( theUnicode, table );

        // Check if the unicode has a translation
        if ( !pSubst ) {
            continue;
        }

        // Check if type is eSkip or substituting string is empty.
        if ( (pSubst->Type ==  eSkip) ||
             !(pSubst->Subst) ) {
            continue;
        }


        // Check if type is eAsIs
        if (pSubst->Type ==  eAsIs) {
            memcpy( pDst, pSrc, utfLen );
//            dstPos += utfLen;
            continue;
        }

        // Check the remaining length and put the result in there.
        size_t substLen = strlen( pSubst->Subst );
        if ( (dstPos + substLen) > dstLen ) {
            return -1; // Unsufficient space;
        }

        // Copy the substituting value into the destignation string
        memcpy( pDst, pSubst->Subst, substLen );
        dstPos += substLen;
    }
    return (int)dstPos;
}


string UTF8ToAsciiString( const char* src, const TUnicodeTable* table)
{
    if ( !src ) return 0;

    string dst;
    size_t srcPos = 0;
    size_t srcLen = strlen( src );

    for ( srcPos = 0; srcPos < srcLen; )
    {
        // Assign quck pointers
        const char* pSrc = &(src[srcPos]);
        TUnicode theUnicode;

        int utfLen = UTF8ToUnicode( pSrc, &theUnicode );

        if ( (utfLen == 0) || (utfLen == -1) ) {
            // Skip the error.
            srcPos++;
            continue;
        }

        srcPos += utfLen;

        // Find the correct substitution.
        const SUnicodeTranslation*
            pSubst = UnicodeToAscii( theUnicode, table );

        // Check if the unicode has a translation
        if ( !pSubst ) {
//            srcPos += utfLen;
            continue;
        }

        // Check if type is eSkip or substituting string is empty.
        if ( (pSubst->Type ==  eSkip) ||
             !(pSubst->Subst) ) {
//            srcPos += utfLen;
            continue;
        }


        // Check if type is eAsIs
        if (pSubst->Type ==  eAsIs) {
            dst += string( pSrc, utfLen );
//            srcPos += utfLen;
            continue;
        }

        // Copy the substituting value into the destignation string
        dst += pSubst->Subst;
    }
    return dst;
}


END_SCOPE(utf8)
END_NCBI_SCOPE

/*
 * ==========================================================================
 * $Log$
 * Revision 1.5  2005/02/07 16:02:28  ivanov
 * Fixed Workshop compiler warnings in 64bit mode
 *
 * Revision 1.4  2004/12/22 18:07:18  osipov
 * Fixed bug: wrong convertation of 0-31 characters
 *
 * Revision 1.3  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2004/05/13 21:18:12  ucko
 * Respect constness in UnicodeToAscii.
 *
 * Revision 1.1  2004/05/06 18:15:29  gouriano
 * Imported from pubmed/xmldb
 *
 * ==========================================================================
 */
