#if defined(STDTYPES__HPP)  &&  !defined(STDTYPES__INL)
#define STDTYPES__INL

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
EPrimitiveValueType CPrimitiveTypeInfo::GetPrimitiveValueType(void) const
{
    return m_ValueType;
}

inline
bool CPrimitiveTypeInfo::IsSigned(void) const
{
    return m_Signed;
}

#if SIZEOF_INT != 4
#  error Unsupported size of int - must be 4 bytes
#endif

inline
int CPrimitiveTypeInfo::GetValueInt(TConstObjectPtr objectPtr) const
{
    return int(GetValueInt4(objectPtr));
}

inline
void CPrimitiveTypeInfo::SetValueInt(TObjectPtr objectPtr, int value) const
{
    SetValueInt4(objectPtr, Int4(value));
}

inline
unsigned CPrimitiveTypeInfo::GetValueUInt(TConstObjectPtr objectPtr) const
{
    return unsigned(GetValueUint4(objectPtr));
}

inline
void CPrimitiveTypeInfo::SetValueUInt(TObjectPtr objectPtr,
                                      unsigned value) const
{
    SetValueUint4(objectPtr, Uint4(value));
}

#if SIZEOF_LONG != 8 && SIZEOF_LONG != 4
#  error Unsupported size of long - must be 4 or 8
#endif

inline
long CPrimitiveTypeInfo::GetValueLong(TConstObjectPtr objectPtr) const
{
#if SIZEOF_LONG == 8
    return long(GetValueInt8(objectPtr));
#else
    return long(GetValueInt4(objectPtr));
#endif
}

inline
void CPrimitiveTypeInfo::SetValueLong(TObjectPtr objectPtr, long value) const
{
#if SIZEOF_LONG == 8
    SetValueInt8(objectPtr, Int8(value));
#else
    SetValueInt4(objectPtr, Int4(value));
#endif
}

inline
unsigned long CPrimitiveTypeInfo::GetValueULong(TConstObjectPtr objectPtr) const
{
#if SIZEOF_LONG == 8
    return static_cast<unsigned long>(GetValueUint8(objectPtr));
#else
    return static_cast<unsigned long>(GetValueUint4(objectPtr));
#endif
}

inline
void CPrimitiveTypeInfo::SetValueULong(TObjectPtr objectPtr,
                                       unsigned long value) const
{
#if SIZEOF_LONG == 8
    SetValueUint8(objectPtr, Uint8(value));
#else
    SetValueUint4(objectPtr, Uint4(value));
#endif
}

#endif /* def STDTYPES__HPP  &&  ndef STDTYPES__INL */
