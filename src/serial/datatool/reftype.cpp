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
*   Type reference defenition
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  1999/11/16 15:41:16  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.3  1999/11/15 19:36:18  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include "reftype.hpp"
#include "code.hpp"
#include "typestr.hpp"
#include "value.hpp"
#include "module.hpp"
#include "exceptions.hpp"
#include "blocktype.hpp"

CReferenceDataType::CReferenceDataType(const string& n)
    : m_UserTypeName(n)
{
}

void CReferenceDataType::SetInSet(void)
{
    CParent::SetInSet();
    CDataType* resolved = ResolveOrNull();
    if ( resolved )
        resolved->SetInSet();
}

void CReferenceDataType::SetInChoice(const CChoiceDataType* choice)
{
    CParent::SetInChoice(choice);
    if ( !choice->GetParentType() ) {
        CDataType* resolved = ResolveOrNull();
        if ( resolved )
            resolved->SetInChoice(choice);
    }
}

void CReferenceDataType::PrintASN(CNcbiOstream& out, int ) const
{
    out << m_UserTypeName;
}

bool CReferenceDataType::CheckType(void) const
{
    try {
        GetModule()->InternalResolve(m_UserTypeName);
        return true;
    }
    catch ( CTypeNotFound& /* ignored */ ) {
        Warning("Unresolved type: " + m_UserTypeName);
        return false;
    }
}

bool CReferenceDataType::CheckValue(const CDataValue& value) const
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return false;
    return resolved->CheckValue(value);
}

TTypeInfo CReferenceDataType::GetTypeInfo(void)
{
    return ResolveOrThrow()->GetTypeInfo();
}

TObjectPtr CReferenceDataType::CreateDefault(const CDataValue& value) const
{
    return ResolveOrThrow()->CreateDefault(value);
}

void CReferenceDataType::GenerateCode(CClassCode& code) const
{
    if ( GetParentType() == 0 ) {
        // alias type
        // skip members
        code.SetClassType(code.eAlias);
        return;
    }
    CParent::GenerateCode(code);
}

void CReferenceDataType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    const CDataType* userType = ResolveOrThrow();

    userType->GetCType(tType, code);

    string memberType = GetVar("_type");
    if ( !memberType.empty() ) {
        if ( memberType == "*" ) {
            tType.ToPointer();
        }
        else {
            tType.AddHPPInclude(GetTemplateHeader(memberType));
            tType.SetTemplate(GetTemplateNamespace(memberType)+"::"+memberType,
                              GetTemplateMacro(memberType), tType,
                              IsSimplePointerTemplate(memberType));
        }
    }
}

CDataType* CReferenceDataType::ResolveOrNull(void) const
{
    try {
        return GetModule()->InternalResolve(m_UserTypeName);
    }
    catch (CTypeNotFound& /* ignored */) {
        return 0;
    }
}

CDataType* CReferenceDataType::ResolveOrThrow(void) const
{
    try {
        return GetModule()->InternalResolve(m_UserTypeName);
    }
    catch (CTypeNotFound& exc) {
        THROW1_TRACE(CTypeNotFound, LocationString() + ": " + exc.what());
    }
}

CDataType* CReferenceDataType::Resolve(void)
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return this;
    return resolved;
}

const CDataType* CReferenceDataType::Resolve(void) const
{
    CDataType* resolved = ResolveOrNull();
    if ( !resolved )
        return this;
    return resolved;
}
