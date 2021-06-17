#if defined(TYPEREF__HPP)  &&  !defined(TYPEREF__INL)
#define TYPEREF__INL

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
CTypeRef::CTypeRef(void)
    : m_Getter(sx_GetAbort), m_ReturnData(0)
{
}

inline
CTypeRef::CTypeRef(TTypeInfo typeInfo)
    : m_Getter(sx_GetReturn), m_ReturnData(typeInfo)
{
}

inline
CTypeRef::CTypeRef(TGetProc getProc)
    : m_Getter(sx_GetProc), m_ReturnData(0)
{
    m_GetProcData = getProc;
}

inline
CTypeRef::CTypeRef(CTypeInfoSource* source)
    : m_Getter(sx_GetResolve), m_ReturnData(0)
{
    m_ResolveData = source;
}

inline
CTypeRef::CTypeRef(const CTypeRef& typeRef)
    : m_Getter(sx_GetAbort), m_ReturnData(0)
{
    Assign(typeRef);
}

inline
CTypeRef::~CTypeRef(void)
{
    Unref();
}

inline
TTypeInfo CTypeRef::Get(void) const
{
    TTypeInfo ret = m_ReturnData;
    return ret? ret: m_Getter(*this);
}

#endif /* def TYPEREF__HPP  &&  ndef TYPEREF__INL */
