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
#include "exceptions.hpp"
#include "typestr.hpp"
#include "classctx.hpp"
#include "ptrstr.hpp"
#include "namespace.hpp"
#include "type.hpp"
#include "module.hpp"

BEGIN_NCBI_SCOPE


CTypeStrings::CTypeStrings(void)
    : m_DataType(NULL)
{
}

CTypeStrings::CTypeStrings(const CComments& comments)
    : m_DataType(NULL), m_Comments(comments)
{
}

CTypeStrings::CTypeStrings(const string& namespaceName, const CDataType* dataType)
    : m_NamespaceName(namespaceName), m_DataType(dataType)
{
}

CTypeStrings::CTypeStrings(const string& namespaceName,
    const CDataType* dataType, const CComments& comments)
    : m_NamespaceName(namespaceName), m_DataType(dataType), m_Comments(comments)
{
}

CTypeStrings::~CTypeStrings(void)
{
}

void CTypeStrings::SetModuleName(const string& name)
{
    _ASSERT(m_ModuleName.empty());
    m_ModuleName = name;
}

const CNamespace& CTypeStrings::GetNamespace(void) const
{
    return CNamespace::KEmptyNamespace;
}

const string& CTypeStrings::GetEnumName(void) const
{
    NCBI_THROW(CDatatoolException,eIllegalCall,"illegal call");
}

string CTypeStrings::GetModuleName(SInternalNames* names) const
{
    string module_name = GetModuleName();
    names->m_OwnerName.erase();
    names->m_MemberName.erase();
#if 1
    if ( module_name.empty() ) {
        // internal type
        const CDataType* this_type = DataType();
        if ( this_type ) {
            names->m_OwnerName = this_type->IdName();
            SIZE_TYPE dot = names->m_OwnerName.rfind('.');
            if ( dot != NPOS ) {
                names->m_MemberName = names->m_OwnerName.substr(dot+1);
                names->m_OwnerName.resize(dot);
            }
            module_name = this_type->GetModule()->GetName();
        }
    }
#endif
    return module_name;
}

string CTypeStrings::GetInitializer(void) const
{
    return NcbiEmptyString;
}

string CTypeStrings::GetDestructionCode(const string& /*expr*/) const
{
    return NcbiEmptyString;
}

string CTypeStrings::GetResetCode(const string& /*var*/) const
{
    return NcbiEmptyString;
}

string CTypeStrings::GetDefaultCode(const string& var) const
{
    return var;
}

bool CTypeStrings::HaveSpecialRef(void) const
{
    return false;
}

bool CTypeStrings::CanBeKey(void) const
{
    switch ( GetKind() ) {
    case eKindStd:
    case eKindEnum:
    case eKindString:
        return true;
    default:
        return false;
    }
}

bool CTypeStrings::CanBeCopied(void) const
{
    switch ( GetKind() ) {
    case eKindStd:
    case eKindEnum:
    case eKindString:
    case eKindPointer:
    case eKindRef:
        return true;
    default:
        return false;
    }
}

bool CTypeStrings::NeedSetFlag(void) const
{
    switch ( GetKind() ) {
    case eKindPointer:
    case eKindRef:
//    case eKindContainer:
        return false;
    default:
        return true;
    }
}

string CTypeStrings::NewInstance(const string& init,
                                 const string& place) const
{
    CNcbiOstrstream s;
    s << "new";
    if ( GetKind() == eKindObject ) {
        s << place;
    }
    s << ' ' << GetCType(CNamespace::KEmptyNamespace) << '(' << init << ')';
    return CNcbiOstrstreamToString(s);
}

string CTypeStrings::GetIsSetCode(const string& /*var*/) const
{
    NCBI_THROW(CDatatoolException,eIllegalCall, "illegal call");
}

void CTypeStrings::AdaptForSTL(AutoPtr<CTypeStrings>& type)
{
    switch ( type->GetKind() ) {
    case eKindStd:
    case eKindEnum:
    case eKindString:
    case eKindPointer:
    case eKindRef:
        // already suitable for STL
        break;
    case eKindObject:
        type.reset(new CRefTypeStrings(type.release()));
        break;
    default:
        if ( !type->CanBeCopied()  ||  !type->CanBeKey()) {
            type.reset(new CPointerTypeStrings(type.release()));
        }
        break;
    }
}

void CTypeStrings::GenerateCode(CClassContext& ctx) const
{
    GenerateTypeCode(ctx);
}

void CTypeStrings::GenerateTypeCode(CClassContext& /*ctx*/) const
{
}

void CTypeStrings::GeneratePointerTypeCode(CClassContext& ctx) const
{
    GenerateTypeCode(ctx);
}

void CTypeStrings::GenerateUserHPPCode(CNcbiOstream& /*out*/) const
{
}

void CTypeStrings::GenerateUserCPPCode(CNcbiOstream& /*out*/) const
{
}

void CTypeStrings::BeginClassDeclaration(CClassContext& ctx) const
{
    CNcbiOstrstream hpp;
    hpp <<
        "/////////////////////////////////////////////////////////////////////////////\n";
    m_Comments.PrintHPPClass(hpp);
    ctx.AddHPPCode(hpp);
}

END_NCBI_SCOPE
