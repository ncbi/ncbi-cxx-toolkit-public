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
*   Type description for CHOIE type
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/12/03 21:42:11  vasilche
* Fixed conflict of enums in choices.
*
* ===========================================================================
*/

#include "choicetype.hpp"
#include <serial/autoptrinfo.hpp>
#include "enumtype.hpp"
#include "value.hpp"
#include "code.hpp"
#include "typestr.hpp"

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
        if ( variantResolved->InheritFromType() == this &&
             dynamic_cast<const CEnumDataType*>(variantResolved) == 0 ) {
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

void CChoiceDataType::GetRefCType(CTypeStrings& tType, CClassCode& ) const
{
    string className = ClassName();
    string ns = Namespace();
    tType.SetClass(ns + "::" + className);
    tType.AddHPPInclude(FileName());
    tType.AddForwardDeclaration(className, ns);
    tType.ToPointer();
    tType.SetChoice();
}

void CChoiceDataType::GetFullCType(CTypeStrings& tType, CClassCode& code) const
{
    if ( GetParentType() == 0 ) {
        // root choice: invalid
        CParent::GetFullCType(tType, code);
        return;
    }

    // generate internal class for internal choice
    GetRefCType(tType, code);
}
