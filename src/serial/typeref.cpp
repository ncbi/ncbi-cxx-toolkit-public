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
* Revision 1.2  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.1  1999/06/04 20:51:52  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeref.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

CTypeInfoSource::CTypeInfoSource(void)
    : m_RefCount(1)
{
}

CTypeInfoSource::~CTypeInfoSource(void)
{
    _ASSERT(m_RefCount == 0);
}

CGetTypeInfoSource::CGetTypeInfoSource(TTypeInfo (*getter)(void))
    : m_Getter(getter)
{
}

TTypeInfo CGetTypeInfoSource::GetTypeInfo(void)
{
    return m_Getter();
}

CGet1TypeInfoSource::CGet1TypeInfoSource(TTypeInfo (*getter)(TTypeInfo ),
                                         const CTypeRef& arg)
    : m_Getter(getter), m_Argument(arg)
{
}

TTypeInfo CGet1TypeInfoSource::GetTypeInfo(void)
{
    return m_Getter(m_Argument.Get());
}

TTypeInfo CTypeRef::sx_Abort(const CTypeRef& )
{
    THROW1_TRACE(runtime_error, "uninitialized type ref");
}

TTypeInfo CTypeRef::sx_Return(const CTypeRef& typeRef)
{
    return typeRef.m_TypeInfo;
}

TTypeInfo CTypeRef::sx_Resolve(const CTypeRef& typeRef)
{
    TTypeInfo typeInfo = typeRef.m_Source->GetTypeInfo();
    if ( !typeInfo )
        THROW1_TRACE(runtime_error, "cannot resolve type ref");
    typeRef.m_TypeInfo = typeInfo;
    typeRef.m_Getter = sx_Return;
    typeRef.Unref();
    return typeInfo;
}

END_NCBI_SCOPE
