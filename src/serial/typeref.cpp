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
* Revision 1.5  2000/09/26 17:38:23  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.4  1999/12/17 19:05:05  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.3  1999/09/14 18:54:22  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.2  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.1  1999/06/04 20:51:52  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <serial/typeref.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

CTypeRef::CTypeRef(CTypeInfoSource* source)
    : m_Getter(sx_Abort)
{
    m_Resolve = source;
    m_Getter = sx_Resolve;
}

CTypeRef::CTypeRef(TGet1Proc getProc, const CTypeRef& arg)
    : m_Getter(sx_Abort)
{
    m_Resolve = new CGet1TypeInfoSource(getProc, arg);
    m_Getter = sx_Resolve;
}

CTypeRef::CTypeRef(TGet2Proc getProc,
                   const CTypeRef& arg1, const CTypeRef& arg2)
    : m_Getter(sx_Abort)
{
    m_Resolve = new CGet2TypeInfoSource(getProc, arg1, arg2);
    m_Getter = sx_Resolve;
}

CTypeRef::CTypeRef(const CTypeRef& typeRef)
    : m_Getter(sx_Abort)
{
    Assign(typeRef);
}

CTypeRef::~CTypeRef(void)
{
    Unref();
}

CTypeRef& CTypeRef::operator=(const CTypeRef& typeRef)
{
    if ( this != &typeRef ) {
        Unref();
        Assign(typeRef);
    }
    return *this;
}

void CTypeRef::Unref(void)
{
    if ( m_Getter == sx_Resolve ) {
        m_Getter = sx_Abort;
        if ( --m_Resolve->m_RefCount <= 0 ) {
            delete m_Resolve;
        }
    }
    else {
        m_Getter = sx_Abort;
    }
}

void CTypeRef::Assign(const CTypeRef& typeRef)
{
    if ( typeRef.m_Getter == sx_Return ) {
        m_Return = typeRef.m_Return;
        m_Getter = sx_Return;
    }
    else if ( typeRef.m_Getter == sx_GetProc ) {
        m_GetProc = typeRef.m_GetProc;
        m_Getter = sx_GetProc;
    }
    else if ( typeRef.m_Getter == sx_Resolve ) {
        ++(m_Resolve = typeRef.m_Resolve)->m_RefCount;
        m_Getter = sx_Resolve;
    }
}

TTypeInfo CTypeRef::sx_Abort(const CTypeRef& )
{
    THROW1_TRACE(runtime_error, "uninitialized type ref");
}

TTypeInfo CTypeRef::sx_Return(const CTypeRef& typeRef)
{
    return typeRef.m_Return;
}

TTypeInfo CTypeRef::sx_GetProc(const CTypeRef& typeRef)
{
    TTypeInfo typeInfo = typeRef.m_GetProc();
    if ( !typeInfo )
        THROW1_TRACE(runtime_error, "cannot resolve type ref");
    const_cast<CTypeRef&>(typeRef).m_Return = typeInfo;
    const_cast<CTypeRef&>(typeRef).m_Getter = sx_Return;
    return typeInfo;
}

TTypeInfo CTypeRef::sx_Resolve(const CTypeRef& typeRef)
{
    TTypeInfo typeInfo = typeRef.m_Resolve->GetTypeInfo();
    if ( !typeInfo )
        THROW1_TRACE(runtime_error, "cannot resolve type ref");
    const_cast<CTypeRef&>(typeRef).Unref();
    const_cast<CTypeRef&>(typeRef).m_Return = typeInfo;
    const_cast<CTypeRef&>(typeRef).m_Getter = sx_Return;
    return typeInfo;
}

CTypeInfoSource::CTypeInfoSource(void)
    : m_RefCount(1)
{
}

CTypeInfoSource::~CTypeInfoSource(void)
{
    _ASSERT(m_RefCount == 0);
}

CGet1TypeInfoSource::CGet1TypeInfoSource(CTypeRef::TGet1Proc getter,
                                         const CTypeRef& arg)
    : m_Getter(getter), m_Argument(arg)
{
}

CGet1TypeInfoSource::~CGet1TypeInfoSource(void)
{
}

TTypeInfo CGet1TypeInfoSource::GetTypeInfo(void)
{
    return m_Getter(m_Argument.Get());
}

CGet2TypeInfoSource::CGet2TypeInfoSource(CTypeRef::TGet2Proc getter,
                                         const CTypeRef& arg1,
                                         const CTypeRef& arg2)
    : m_Getter(getter), m_Argument1(arg1), m_Argument2(arg2)
{
}

CGet2TypeInfoSource::~CGet2TypeInfoSource(void)
{
}

TTypeInfo CGet2TypeInfoSource::GetTypeInfo(void)
{
    return m_Getter(m_Argument1.Get(), m_Argument2.Get());
}

END_NCBI_SCOPE
