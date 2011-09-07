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
*/

#include <corelib/ncbistd.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

#if defined(_DEBUG)
#  define CHECK_INSTREAM_STATE      1
#  define CHECK_INSTREAM_LIMITS     1
#  define CHECK_OUTSTREAM_INTEGRITY 1
#else
#  define CHECK_INSTREAM_STATE      0
#  define CHECK_INSTREAM_LIMITS     0
#  define CHECK_OUTSTREAM_INTEGRITY 0
#endif

class NCBI_XSERIAL_EXPORT CAsnBinaryDefs
{
public:
    typedef Uint1 TByte;
    typedef Int4 TLongTag;

    enum ETagClass {
        eUniversal          = 0 << 6,
        eApplication        = 1 << 6,
        eContextSpecific    = 2 << 6,
        ePrivate            = 3 << 6,
        eTagClassMask       = 3 << 6
    };

    enum ETagConstructed {
        ePrimitive          = 0 << 5,
        eConstructed        = 1 << 5,
        eTagConstructedMask = 1 << 5
    };

    enum ETagValue {
        eNone               = 0,
        eBoolean            = 1,
        eInteger            = 2,
        eBitString          = 3,
        eOctetString        = 4,
        eNull               = 5,
        eObjectIdentifier   = 6,
        eObjectDescriptor   = 7,
        eExternal           = 8,
        eReal               = 9,
        eEnumerated         = 10,
        
        eUTF8String         = 12,

        eSequence           = 16,
        eSequenceOf         = eSequence,
        eSet                = 17,
        eSetOf              = eSet,
        eNumericString      = 18,
        ePrintableString    = 19,
        eTeletextString     = 20,
        eT61String          = 20,
        eVideotextString    = 21,
        eIA5String          = 22,

        eUTCTime            = 23,
        eGeneralizedTime    = 24,

        eGraphicString      = 25,
        eVisibleString      = 26,
        eISO646String       = 26,
        eGeneralString      = 27,

        eMemberReference    = 29, // non standard, use with eApplication class
        eObjectReference    = 30, // non standard, use with eApplication class

        eLongTag            = 31,

        eStringStore        = 1, // non standard, use with eApplication class

        eTagValueMask       = 31
    };

    enum ESpecialOctets {
        // combined bytes
        eContainterTagByte      = TByte(eConstructed) | TByte(eSequence),
        eIndefiniteLengthByte   = 0x80,
        eEndOfContentsByte      = 0,
        eZeroLengthByte         = 0
    };

    enum ERealRadix {
        eDecimal            = 0
    };


    static TByte MakeTagByte(ETagClass tag_class,
                             ETagConstructed tag_constructed,
                             ETagValue tag_value);
    static TByte MakeTagClassAndConstructed(ETagClass tag_class,
                                            ETagConstructed tag_constructed);
    static TByte MakeContainerTagByte(bool random_order);
    static ETagValue GetTagValue(TByte byte);
    static ETagValue StringTag(EStringType type);
    static ETagConstructed GetTagConstructed(TByte byte);
    static TByte GetTagClassAndConstructed(TByte byte);
};

#include <serial/impl/objstrasnb.inl>

END_NCBI_SCOPE

/* @} */

#endif  /* OBJSTRASNB__HPP */
