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
*   Type description for compound types: SET, SEQUENCE and CHOICE
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  1999/12/01 17:36:24  vasilche
* Fixed CHOICE processing.
*
* Revision 1.5  1999/11/18 17:13:05  vasilche
* Fixed generation of ENUMERATED CHOICE and VisibleString.
* Added generation of initializers to zero for primitive types and pointers.
*
* Revision 1.4  1999/11/16 15:41:16  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.3  1999/11/15 20:31:37  vasilche
* Fixed error on GCC
*
* Revision 1.2  1999/11/15 19:36:13  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/choice.hpp>
#include <serial/autoptrinfo.hpp>
#include "blocktype.hpp"
#include "statictype.hpp"
#include "code.hpp"
#include "typestr.hpp"
#include "value.hpp"

class CContainerTypeInfo : public CClassInfoTmpl
{
public:
    CContainerTypeInfo(const string& name, size_t size)
        : CClassInfoTmpl(name, typeid(void), MemberOffset(size)),
          m_Size(size)
        {
        }

    TObjectPtr Create(void) const
        {
            return new AnyType[m_Size];
        }

    static size_t MemberOffset(size_t index)
        {
            return sizeof(AnyType) * index;
        }

private:
    size_t m_Size;
};

void CDataMemberContainerType::AddMember(const AutoPtr<CDataMember>& member)
{
    m_Members.push_back(member);
}

void CDataMemberContainerType::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetASNKeyword() << " {";
    indent++;
    for ( TMembers::const_iterator i = m_Members.begin();
          i != m_Members.end(); ++i ) {
        if ( i != m_Members.begin() )
            out << ',';
        NewLine(out, indent);
        (*i)->PrintASN(out, indent);
    }
    NewLine(out, indent - 1);
    out << "}";
}

void CDataMemberContainerType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    iterate ( TMembers, i, m_Members ) {
        (*i)->GetType()->SetParent(this, (*i)->GetName());
    }
}

bool CDataMemberContainerType::CheckType(void) const
{
    bool ok = true;
    iterate ( TMembers, i, m_Members ) {
        if ( !(*i)->Check() )
            ok = false;
    }
    return ok;
}

TObjectPtr CDataMemberContainerType::CreateDefault(const CDataValue& ) const
{
    THROW1_TRACE(runtime_error,
                 GetASNKeyword() + string(" default not implemented"));
}

CTypeInfo* CDataContainerType::CreateTypeInfo(void)
{
    return CreateClassInfo();
}

CClassInfoTmpl* CDataContainerType::CreateClassInfo(void)
{
    auto_ptr<CClassInfoTmpl> typeInfo(new CContainerTypeInfo(IdName(),
                                                      GetMembers().size()));
    int index = 0;
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i, ++index ) {
        CDataMember* member = i->get();
        CMemberInfo* memberInfo =
            typeInfo->AddMember(member->GetName(),
                new CRealMemberInfo(CContainerTypeInfo::MemberOffset(index),
                                    new CAnyTypeSource(member->GetType())));
        if ( member->GetDefault() ) {
            memberInfo->SetDefault(member->GetType()->
                                   CreateDefault(*member->GetDefault()));
        }
        else if ( member->Optional() ) {
            memberInfo->SetOptional();
        }
    }
    return typeInfo.release();
}

void CDataContainerType::GenerateCode(CClassCode& code) const
{
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end();
          ++i ) {
        const CDataMember* m = i->get();
        if ( m->GetName().empty() )
            continue;
        
        CTypeStrings tType;
        m->GetType()->GetCType(tType, code);

        string memberType = GetVar(m->GetName() + "._pointer_type");
        if ( !memberType.empty() ) {
            if ( memberType == "*" ) {
                tType.ToPointer();
            }
            else {
                tType.AddHPPInclude(GetTemplateHeader(memberType));
                tType.ToPointer(
                    GetTemplateNamespace(memberType)+"::"+memberType,
                    GetTemplateMacro(memberType),
                    IsSimplePointerTemplate(memberType));
            }
        }

        tType.AddMember(code, m->GetName(), "m_" + Identifier(m->GetName()));

        if ( m->GetDefault() ) {
            code.TypeInfoBody() << "->SetDefault(" <<
                m->GetType()->GetDefaultString(*m->GetDefault()) << ')';
        }
        else if ( m->Optional() ) {
            code.TypeInfoBody() << "->SetOptional()";
        }
        code.TypeInfoBody() << ';' << NcbiEndl;
    }
}

void CDataContainerType::GetCType(CTypeStrings& tType, CClassCode& ) const
{
    string className = ClassName();
    tType.SetClass(className);
    tType.AddHPPInclude(FileName());
    tType.AddForwardDeclaration(className, Namespace());
}

const char* CDataSetType::GetASNKeyword(void) const
{
    return "SET";
}

bool CDataSetType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }

    typedef map<string, const CDataMember*> TReadValues;
    TReadValues mms;
    for ( TMembers::const_iterator m = GetMembers().begin();
          m != GetMembers().end(); ++m ) {
        mms[m->get()->GetName()] = m->get();
    }

    iterate ( CBlockDataValue::TValues, v, block->GetValues() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(v->get());
        if ( !currvalue ) {
            v->get()->Warning("named value expected");
            return false;
        }
        TReadValues::iterator member = mms.find(currvalue->GetName());
        if ( member == mms.end() ) {
            currvalue->Warning("unexpected member");
            return false;
        }
        if ( !member->second->GetType()->CheckValue(currvalue->GetValue()) ) {
            return false;
        }
        mms.erase(member);
    }
    
    for ( TReadValues::const_iterator member = mms.begin();
          member != mms.end(); ++member ) {
        if ( !member->second->Optional() ) {
            value.Warning(member->first + " member expected");
            return false;
        }
    }
    return true;
}

CClassInfoTmpl* CDataSetType::CreateClassInfo(void)
{
    return CParent::CreateClassInfo()->SetRandomOrder();
}

const char* CDataSequenceType::GetASNKeyword(void) const
{
    return "SEQUENCE";
}

bool CDataSequenceType::CheckValue(const CDataValue& value) const
{
    const CBlockDataValue* block =
        dynamic_cast<const CBlockDataValue*>(&value);
    if ( !block ) {
        value.Warning("block of values expected");
        return false;
    }
    TMembers::const_iterator member = GetMembers().begin();
    CBlockDataValue::TValues::const_iterator cvalue =
        block->GetValues().begin();
    while ( cvalue != block->GetValues().end() ) {
        const CNamedDataValue* currvalue =
            dynamic_cast<const CNamedDataValue*>(cvalue->get());
        if ( !currvalue ) {
            cvalue->get()->Warning("named value expected");
            return false;
        }
        for (;;) {
            if ( member == GetMembers().end() ) {
                currvalue->Warning("unexpected value");
                return false;
            }
            if ( (*member)->GetName() == currvalue->GetName() )
                break;
            if ( !(*member)->Optional() ) {
                currvalue->GetValue().Warning((*member)->GetName() +
                                              " member expected");
                return false;
            }
            ++member;
        }
        if ( !(*member)->GetType()->CheckValue(currvalue->GetValue()) ) {
            return false;
        }
        ++member;
        ++cvalue;
    }
    while ( member != GetMembers().end() ) {
        if ( !(*member)->Optional() ) {
            value.Warning((*member)->GetName() + " member expected");
            return false;
        }
    }
    return true;
}

const char* CChoiceDataType::GetASNKeyword(void) const
{
    return "CHOICE";
}

void CChoiceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    iterate ( TMembers, m, GetMembers() ) {
        (*m)->GetType()->SetInChoice(this);
    }
}

bool CChoiceDataType::CheckValue(const CDataValue& value) const
{
    const CNamedDataValue* choice =
        dynamic_cast<const CNamedDataValue*>(&value);
    if ( !choice ) {
        value.Warning("CHOICE value expected");
        return false;
    }
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        if ( (*i)->GetName() == choice->GetName() )
            return (*i)->GetType()->CheckValue(choice->GetValue());
    }
    return false;
}

CTypeInfo* CChoiceDataType::CreateTypeInfo(void)
{
    auto_ptr<CChoiceTypeInfoBase> typeInfo(
        new CChoiceTypeInfoTmpl<AnyType>(IdName()));
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        CDataMember* member = i->get();
        typeInfo->AddVariant(member->GetName(),
                             new CAnyTypeSource(member->GetType()));
    }
    return new CAutoPointerTypeInfo(typeInfo.release());
}

void CChoiceDataType::GenerateCode(CClassCode& code) const
{
    code.SetClassType(code.eAbstract);
    string className = ClassName();
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end();
          ++i ) {
        const CDataMember* m = i->get();
        string fieldName = Identifier(m->GetName());
        if ( fieldName.empty() ) {
            continue;
        }
        const CDataType* variant = m->GetType();
        const CDataType* variantResolved = variant->Resolve();
        if ( variantResolved->InheritFromType() == this ) {
            // named choice AND variant appears only in this choice
            variant = variantResolved;
        }
        string variantName = Identifier(m->GetName());
        if ( dynamic_cast<const CNullDataType*>(variant) != 0 ) {
            // NULL variant
            code.ClassPublic() <<
                "    bool Is"<<variantName<<"(void) const"<<NcbiEndl<<
                "        { return this == 0; }"<<NcbiEndl<<
                NcbiEndl;
            continue;
        }
        string variantClass = variant->ClassName();
        code.AddCPPInclude(variant->FileName());
        code.AddForwardDeclaration(variantClass, variant->Namespace());
        code.TypeInfoBody() <<
            "    ADD_SUB_CLASS2(\"" << m->GetName() << "\", " << variantClass <<
                   ");" << NcbiEndl;
        code.ClassPublic() << NcbiEndl <<
            "    bool Is"<<variantName<<"(void) const;" << NcbiEndl <<
            "    "<<variantClass<<"* Get"<<variantName<<"(void);" << NcbiEndl <<
            "    const "<<variantClass<<"* Get"<<variantName<<"(void) const;" << NcbiEndl;
        code.Methods() <<
            "bool "<<className<<"_Base::Is"<<variantName<<"(void) const" << NcbiEndl <<
            '{' << NcbiEndl <<
            "    return dynamic_cast<const "<<variantClass<<"*>(this) != 0;" << NcbiEndl <<
            '}' << NcbiEndl <<
            variantClass<<"* "<<className<<"_Base::Get"<<variantName<<"(void)" << NcbiEndl <<
            '{' << NcbiEndl <<
            "    return dynamic_cast<"<<variantClass<<"*>(this);" << NcbiEndl <<
            '}' << NcbiEndl <<
            "const "<<variantClass<<"* "<<className<<"_Base::Get"<<variantName<<"(void) const" << NcbiEndl <<
            '{' << NcbiEndl <<
            "    return dynamic_cast<const "<<variantClass<<"*>(this);" << NcbiEndl <<
            '}' << NcbiEndl;
    }
}

void CChoiceDataType::GetCType(CTypeStrings& tType, CClassCode& ) const
{
    string className = ClassName();
    tType.SetClass(className);
    tType.AddHPPInclude(FileName());
    tType.AddForwardDeclaration(className, Namespace());
    tType.ToPointer();
    tType.SetChoice();
}

CDataMember::CDataMember(const string& name, const AutoPtr<CDataType>& type)
    : m_Name(name), m_Type(type), m_Optional(false)
{
}

CDataMember::~CDataMember(void)
{
}

void CDataMember::PrintASN(CNcbiOstream& out, int indent) const
{
    out << GetName() << ' ';
    GetType()->PrintASN(out, indent);
    if ( GetDefault() ) {
        GetDefault()->PrintASN(out << " DEFAULT ", indent + 1);
    }
    else if ( Optional() ) {
        out << " OPTIONAL";
    }
}

bool CDataMember::Check(void) const
{
    if ( !m_Type->Check() )
        return false;
    if ( !m_Default )
        return true;
    return GetType()->CheckValue(*m_Default);
}

void CDataMember::SetDefault(const AutoPtr<CDataValue>& value)
{
    m_Default = value;
}

void CDataMember::SetOptional(void)
{
    m_Optional = true;
}
