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
    catch ( CTypeNotFound& exc ) {
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
            tType.ToSimple();
        }
        else {
            tType.AddHPPInclude(GetTemplateHeader(memberType));
            tType.SetComplex(GetTemplateNamespace(memberType) + "::" + memberType,
                             GetTemplateMacro(memberType), tType);
            if ( IsSimpleTemplate(memberType) )
                tType.type = tType.ePointerType;
        }
    }
}

CDataType* CReferenceDataType::ResolveOrNull(void) const
{
    try {
        return GetModule()->InternalResolve(m_UserTypeName);
    }
    catch (CTypeNotFound& exc) {
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
