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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/18 20:00:08  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

inline
TByte MakeTagByte(EClass c, bool constructed, ETag tag)
{
    return (c << 6) | (constructed? 0x20: 0) | tag;
}
    
inline
TByte MakeTagByte(EClass c, bool constructed)
{
    return MakeTagByte(c, constructed, eNone);
}

inline
ETag ExtractTag(TByte byte)
{
    return ETag(byte & 0x1f);
}

inline
bool ExtractConstructed(TByte byte)
{
    return (byte & 0x20) != 0;
}

inline
TByte ExtractClassAndConstructed(TByte byte)
{
    return byte & 0xE0;
}

#endif /* def OBJSTRASNB__HPP  &&  ndef OBJSTRASNB__INL */
