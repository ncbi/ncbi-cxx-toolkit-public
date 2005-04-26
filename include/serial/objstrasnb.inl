#if defined(OBJSTRASNB__HPP)  &&  !defined(OBJSTRASNB__INL)
#define OBJSTRASNB__INL

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

inline
Uint1 MakeTagByte(EClass c, bool constructed, ETag tag)
{
    return static_cast<Uint1>((c << 6) | (constructed? 0x20: 0) | tag);
}
    
inline
Uint1 MakeTagByte(EClass c, bool constructed)
{
    return static_cast<Uint1>(constructed? (c << 6)|0x20: (c<<6));
}

inline
Uint1 MakeContainerTagByte(bool random_order)
{
    return static_cast<Uint1>(0x30 + random_order);
}

inline
ETag ExtractTag(Uint1 byte)
{
    return ETag(byte & 0x1f);
}

inline
bool ExtractConstructed(Uint1 byte)
{
    return (byte & 0x20) != 0;
}

inline
Uint1 ExtractClassAndConstructed(Uint1 byte)
{
    return static_cast<Uint1>(byte & 0xE0);
}

#endif /* def OBJSTRASNB__HPP  &&  ndef OBJSTRASNB__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2005/04/26 14:13:27  vasilche
* Optimized binary ASN.1 parsing.
*
* Revision 1.4  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2001/08/15 20:53:06  juran
* Heed warnings.
*
* Revision 1.2  2000/12/15 21:28:49  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.1  2000/09/18 20:00:08  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
