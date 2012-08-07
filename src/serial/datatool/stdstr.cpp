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
*   Type info for class generation: includes, used classes, C code etc.
*
*/

#include <ncbi_pch.hpp>
#include "stdstr.hpp"
#include "classctx.hpp"

BEGIN_NCBI_SCOPE

CStdTypeStrings::CStdTypeStrings(const string& type, const CComments& comments, bool full_ns_name)
    : CTypeStrings(comments), m_CType(type)
{
    SIZE_TYPE colon = type.rfind("::");
    if ( colon != NPOS ) {
        m_CType = type.substr(colon + 2);
        m_Namespace = type.substr(0, colon);
        m_Namespace.UseFullname(full_ns_name);
    }
}

CTypeStrings::EKind CStdTypeStrings::GetKind(void) const
{
    return eKindStd;
}

string CStdTypeStrings::GetCType(const CNamespace& ns) const
{
    if ( m_Namespace )
        return ns.GetNamespaceRef(m_Namespace)+m_CType;
    else
        return m_CType;
}

string CStdTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                         const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CStdTypeStrings::GetRef(const CNamespace& ns) const
{
    return "STD, ("+GetCType(ns)+')';
}

string CStdTypeStrings::GetInitializer(void) const
{
    return "0";
}

CNullTypeStrings::CNullTypeStrings(const CComments& comments)
    : CTypeStrings(comments)
{
}

CTypeStrings::EKind CNullTypeStrings::GetKind(void) const
{
    return eKindStd;
}

bool CNullTypeStrings::HaveSpecialRef(void) const
{
    return true;
}

string CNullTypeStrings::GetCType(const CNamespace& /*ns*/) const
{
    return "bool";
}

string CNullTypeStrings::GetPrefixedCType(const CNamespace& ns,
                                          const string& /*methodPrefix*/) const
{
    return GetCType(ns);
}

string CNullTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "null, ()";
}

string CNullTypeStrings::GetInitializer(void) const
{
    return "true";
}

CStringTypeStrings::CStringTypeStrings(const string& type,
    const CComments& comments, bool full_ns_name)
    : CParent(type,comments,full_ns_name)
{
}

CTypeStrings::EKind CStringTypeStrings::GetKind(void) const
{
    return eKindString;
}

string CStringTypeStrings::GetInitializer(void) const
{
    return string();
}

string CStringTypeStrings::GetResetCode(const string& var) const
{
    return var+".erase();\n";
}

void CStringTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert("<string>");
}

CStringStoreTypeStrings::CStringStoreTypeStrings(const string& type,
    const CComments& comments, bool full_ns_name)
    : CParent(type,comments,full_ns_name)
{
}

bool CStringStoreTypeStrings::HaveSpecialRef(void) const
{
    return true;
}

string CStringStoreTypeStrings::GetRef(const CNamespace& /*ns*/) const
{
    return "StringStore, ()";
}

CAnyContentTypeStrings::CAnyContentTypeStrings(const string& type,
    const CComments& comments, bool full_ns_name)
    : CParent(type,comments,full_ns_name)
{
}

CTypeStrings::EKind CAnyContentTypeStrings::GetKind(void) const
{
    return eKindObject;
}

string CAnyContentTypeStrings::GetInitializer(void) const
{
    return string();
}

string CAnyContentTypeStrings::GetResetCode(const string& var) const
{
    return var+".Reset();\n";
}

void CAnyContentTypeStrings::GenerateTypeCode(CClassContext& /*ctx*/) const
{
}


CBitStringTypeStrings::CBitStringTypeStrings(const string& type,
    const CComments& comments)
    : CParent(type,comments,false)
{
}

CTypeStrings::EKind CBitStringTypeStrings::GetKind(void) const
{
    return eKindOther;
}

string CBitStringTypeStrings::GetInitializer(void) const
{
    return string();
}

string CBitStringTypeStrings::GetResetCode(const string& var) const
{
//    return var+".clear();\n";
    return var+".resize(0);\n";
}

void CBitStringTypeStrings::GenerateTypeCode(CClassContext& ctx) const
{
    ctx.HPPIncludes().insert("<vector>");
}

END_NCBI_SCOPE
