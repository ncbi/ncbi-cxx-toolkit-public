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
CAsnBinaryDefs::TByte
CAsnBinaryDefs::MakeTagByte(ETagClass tag_class,
                            ETagConstructed tag_constructed,
                            ETagValue tag_value)
{
    return TByte(TByte(tag_class) | TByte(tag_value) | TByte(tag_constructed));
}
    
inline
CAsnBinaryDefs::TByte
CAsnBinaryDefs::MakeTagClassAndConstructed(ETagClass tag_class,
                                           ETagConstructed tag_constructed)
{
    return TByte(TByte(tag_class) | TByte(tag_constructed));
}

inline
CAsnBinaryDefs::TByte
CAsnBinaryDefs::MakeContainerTagByte(bool random_order)
{
    return TByte(eContainterTagByte + random_order);
}

inline
CAsnBinaryDefs::ETagValue
CAsnBinaryDefs::StringTag(EStringType type)
{
    return eVisibleString;
    //return type == eStringTypeVisible? eVisibleString: eUTF8String;
}

inline
CAsnBinaryDefs::ETagValue
CAsnBinaryDefs::GetTagValue(TByte tag_byte)
{
    return ETagValue(tag_byte & eTagValueMask);
}

inline
CAsnBinaryDefs::ETagConstructed
CAsnBinaryDefs::GetTagConstructed(TByte tag_byte)
{
    return ETagConstructed(tag_byte & eTagConstructedMask);
}

inline
CAsnBinaryDefs::TByte
CAsnBinaryDefs::GetTagClassAndConstructed(TByte tag_byte)
{
    return TByte(tag_byte & (TByte(eTagClassMask)|TByte(eTagConstructedMask)));
}

#endif /* def OBJSTRASNB__HPP  &&  ndef OBJSTRASNB__INL */
