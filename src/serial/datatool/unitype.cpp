#include <serial/stltypes.hpp>
#include <serial/autoptrinfo.hpp>
#include "unitype.hpp"
#include "blocktype.hpp"
#include "code.hpp"
#include "typestr.hpp"
#include "value.hpp"

CUniSequenceDataType::CUniSequenceDataType(const AutoPtr<CDataType>& elementType)
{
    SetElementType(elementType);
}

const char* CUniSequenceDataType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

void CUniSequenceDataType::SetElementType(const AutoPtr<CDataType>& type)
{
    if ( GetElementType() )
        THROW1_TRACE(runtime_error, "double element type " + LocationString());
    m_ElementType = type;
}

void CUniSequenceDataType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " OF ";
    GetElementType()->PrintASN(out, indent);
}

void CUniSequenceDataType::FixTypeTree(void) const
{
    m_ElementType->SetParent(this, "_E");
}

bool CUniSequenceDataType::CheckType(void) const
{
    return m_ElementType->Check();
}

bool CUniSequenceDataType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block = dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    bool ok = true;
    for ( CBlockDataValue::TValues::const_iterator i = block->GetValues().begin();
          i != block->GetValues().end(); ++i ) {
        if ( !m_ElementType->CheckValue(**i) )
            ok = false;
    }
    return ok;
}

TObjectPtr CUniSequenceDataType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error, "SET/SEQUENCE OF default not implemented");
}

CTypeInfo* CUniSequenceDataType::CreateTypeInfo(void)
{
    return new CAutoPointerTypeInfo(
        new CStlClassInfoList<AnyType>(new CAnyTypeSource(m_ElementType.get())));
}

void CUniSequenceDataType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string templ = GetVar("_type");
    CTypeStrings tData;
    GetElementType()->GetCType(tData, code);
    tData.ToSimple();
    if ( templ.empty() )
        templ = "list";
    tType.AddHPPInclude(GetTemplateHeader(templ));
    tType.SetComplex(GetTemplateNamespace(templ) + "::" + templ,
                     GetTemplateMacro(templ), tData);
}

CUniSetDataType::CUniSetDataType(const AutoPtr<CDataType>& elementType)
    : CParent(elementType)
{
}

const char* CUniSetDataType::GetASNKeyword(void) const
{
    return "SET";
}

CTypeInfo* CUniSetDataType::CreateTypeInfo(void)
{
    CStlClassInfoList<AnyType>* l =
        new CStlClassInfoList<AnyType>(new CAnyTypeSource(GetElementType()));
    l->SetRandomOrder();
    return new CAutoPointerTypeInfo(l);
}

void CUniSetDataType::GetCType(CTypeStrings& tType, CClassCode& code) const
{
    string templ = GetVar("_type");
    const CDataSequenceType* seq =
        dynamic_cast<const CDataSequenceType*>(GetElementType());
    if ( seq && seq->GetMembers().size() == 2 ) {
        CTypeStrings tKey;
        seq->GetMembers().front()->GetType()->GetCType(tKey, code);
        if ( tKey.type == tKey.eStdType ) {
            CTypeStrings tValue;
            seq->GetMembers().back()->GetType()->GetCType(tValue, code);
            tValue.ToSimple();
            if ( templ.empty() )
                templ = "multimap";

            tType.AddHPPInclude(GetTemplateHeader(templ));
            tType.SetComplex(GetTemplateNamespace(templ) + "::" + templ,
                             GetTemplateMacro(templ), tKey, tValue);
            return;
        }
    }
    CTypeStrings tData;
    GetElementType()->GetCType(tData, code);
    tData.ToSimple();
    if ( templ.empty() )
        templ = "multiset";
    tType.AddHPPInclude(GetTemplateHeader(templ));
    tType.SetComplex(GetTemplateNamespace(templ) + "::" + templ,
                     GetTemplateMacro(templ), tData);
}

