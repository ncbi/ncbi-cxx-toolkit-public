#ifndef OBJSTRASNB__HPP
#define OBJSTRASNB__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/07/21 14:20:01  vasilche
* Added serialization of bool.
*
* Revision 1.1  1999/07/02 21:31:49  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

namespace CObjectStreamAsnBinaryDefs
{
    typedef unsigned char TByte;
    typedef int TTag;

    enum EClass {
        eUniversal,
        eApplication,
        eContextSpecific,
        ePrivate
    };

    enum ETag {
        eNone = 0,
        eBoolean = 1,
        eInteger = 2,
        eBitString = 3,
        eOctetString = 4,
        eNull = 5,
        eObjectIdentifier = 6,
        eObjectDescriptor = 7,
        eExternal = 8,
        eReal = 9,
        eEnumerated = 10,

        eSequence = 16,
        eSequenceOf = eSequence,
        eSet = 17,
        eSetOf = eSet,
        eNumericString = 18,
        ePrintableString = 19,
        eTeletextString = 20,
        eT61String = 20,
        eVideotextString = 21,
        eIA5String = 22,

        eUTCTime = 23,
        eGeneralizedTime = 24,

        eGraphicString = 25,
        eVisibleString = 26,
        eISO646String = 26,
        eGeneralString = 27,

        eLongTag = 31
    };

    enum ERealRadix {
        eDecimal = 0
    };
}

END_NCBI_SCOPE

#endif  /* OBJSTRASNB__HPP */
