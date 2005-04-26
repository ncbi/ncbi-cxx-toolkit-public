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

#ifdef _DEBUG
#define CHECK_STREAM_INTEGRITY 1
#else
#undef CHECK_STREAM_INTEGRITY
#endif

namespace CObjectStreamAsnBinaryDefs
{
    typedef Int4 TTag;

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

        eMemberReference = 29, // non standard use with eApplication class
        eObjectReference = 30, // non standard use with eApplication class

        eLongTag = 31,

        // eApplication class
        eStringStore = 1,

        // combined bytes
        eEndOfContentsByte = 0
    };

    enum ERealRadix {
        eDecimal = 0
    };


    inline Uint1 MakeTagByte(EClass c, bool constructed, ETag tag);
    inline Uint1 MakeTagByte(EClass c, bool constructed);
    inline Uint1 MakeContainerTagByte(bool random_order);
    inline ETag ExtractTag(Uint1 byte);
    inline bool ExtractConstructed(Uint1 byte);
    inline Uint1 ExtractClassAndConstructed(Uint1 byte);


/* @} */


#include <serial/objstrasnb.inl>

}

END_NCBI_SCOPE

#endif  /* OBJSTRASNB__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2005/04/26 14:13:27  vasilche
* Optimized binary ASN.1 parsing.
*
* Revision 1.9  2003/04/15 16:18:35  siyan
* Added doxygen support
*
* Revision 1.8  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.7  2000/12/15 21:28:49  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.6  2000/09/18 20:00:08  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.5  2000/02/17 20:02:29  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.4  2000/01/10 19:46:33  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.3  1999/07/22 17:33:47  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.2  1999/07/21 14:20:01  vasilche
* Added serialization of bool.
*
* Revision 1.1  1999/07/02 21:31:49  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/
