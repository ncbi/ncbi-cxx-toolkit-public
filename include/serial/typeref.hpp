#ifndef TYPEREF__HPP
#define TYPEREF__HPP

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
* Revision 1.4  1999/07/13 20:18:12  vasilche
* Changed types naming.
*
* Revision 1.3  1999/06/24 14:44:47  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/09 18:39:01  vasilche
* Modified templates to work on Sun.
*
* Revision 1.1  1999/06/04 20:51:41  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>

BEGIN_NCBI_SCOPE

class CTypeRef
{
public:
    CTypeRef(void)
        : m_Resolver(s_Abort)
        { }
    CTypeRef(TTypeInfo typeInfo)
        : m_Resolver(s_Return), m_TypeInfo(typeInfo)
        { }
    CTypeRef(TTypeInfo (*getter)(void))
        : m_Resolver(s_Resolve0), m_Getter0(getter)
        { }
    CTypeRef(TTypeInfo (*getter)(TTypeInfo typeInfo), const CTypeRef& arg)
        : m_Resolver(s_Resolve1), m_TypeInfo(arg.Get()), m_Getter1(getter)
        { }

    TTypeInfo Get(void) const
        { return m_Resolver(*this); }

private:

    static TTypeInfo s_Abort(const CTypeRef& )
        {
            THROW1_TRACE(runtime_error, "uninitialized type ref");
        }
    static TTypeInfo s_Return(const CTypeRef& typeRef)
        {
            return typeRef.m_TypeInfo;
        }
    static TTypeInfo s_Resolve0(const CTypeRef& typeRef)
        {
            TTypeInfo typeInfo = typeRef.m_Getter0();
            if ( !typeInfo )
                THROW1_TRACE(runtime_error, "cannot resolve type ref");
            typeRef.m_TypeInfo = typeInfo;
            typeRef.m_Resolver = s_Return;
            return typeInfo;
        }
    static TTypeInfo s_Resolve1(const CTypeRef& typeRef)
        {
            TTypeInfo typeInfo = typeRef.m_Getter1(typeRef.m_TypeInfo);
            if ( !typeInfo )
                THROW1_TRACE(runtime_error, "cannot resolve type ref");
            typeRef.m_TypeInfo = typeInfo;
            typeRef.m_Resolver = s_Return;
            return typeInfo;
        }

    mutable TTypeInfo (*m_Resolver)(const CTypeRef& typeRef);
    mutable TTypeInfo m_TypeInfo;
    union {
        TTypeInfo (*m_Getter0)(void);
        TTypeInfo (*m_Getter1)(TTypeInfo arg);
    };
};

#include <serial/typeref.inl>

END_NCBI_SCOPE

#endif  /* TYPEREF__HPP */
