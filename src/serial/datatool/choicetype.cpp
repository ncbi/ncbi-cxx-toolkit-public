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
* Revision 1.8  2000/04/07 19:26:24  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.7  2000/03/07 14:06:30  vasilche
* Added generation of reference counted objects.
*
* Revision 1.6  2000/02/01 21:47:55  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  2000/01/05 19:43:59  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.4  1999/12/28 18:55:56  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/21 17:18:33  vasilche
* Added CDelayedFostream class which rewrites file only if contents is changed.
*
* Revision 1.2  1999/12/17 19:05:18  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.1  1999/12/03 21:42:11  vasilche
* Fixed conflict of enums in choices.
*
* ===========================================================================
*/

#include <serial/tool/choicetype.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/tool/value.hpp>
#include <serial/tool/choicestr.hpp>

BEGIN_NCBI_SCOPE

CChoiceTypeInfoAnyType::CChoiceTypeInfoAnyType(const string& name)
    : CParent(name)
{
}

CChoiceTypeInfoAnyType::CChoiceTypeInfoAnyType(const char* name)
    : CParent(name)
{
}

CChoiceTypeInfoAnyType::~CChoiceTypeInfoAnyType(void)
{
}

size_t CChoiceTypeInfoAnyType::GetSize(void) const
{
    return TType::GetSize();
}

TObjectPtr CChoiceTypeInfoAnyType::Create(void) const
{
    TObjectType* obj = new TObjectType;
    obj->index = -1;
    return obj;
}

CChoiceTypeInfoAnyType::TMemberIndex
CChoiceTypeInfoAnyType::GetIndex(TConstObjectPtr object) const
{
    return Get(object).index;
}

void CChoiceTypeInfoAnyType::SetIndex(TObjectPtr object,
                                      TMemberIndex index) const
{
    Get(object).index = index;
}

TObjectPtr CChoiceTypeInfoAnyType::x_GetData(TObjectPtr object,
                                             TMemberIndex /*index*/) const
{
    return &Get(object).data;
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
        new CChoiceTypeInfoAnyType(IdName()));
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        CDataMember* member = i->get();
        typeInfo->AddVariant(member->GetName(),
                             new CAnyTypeSource(member->GetType()));
    }
    return new CAutoPointerTypeInfo(typeInfo.release());
}

AutoPtr<CTypeStrings> CChoiceDataType::GenerateCode(void) const
{
    return GetFullCType();
}

AutoPtr<CTypeStrings> CChoiceDataType::GetRefCType(void) const
{
    return AutoPtr<CTypeStrings>(new CChoiceRefTypeStrings(ClassName(),
                                                           Namespace(),
                                                           FileName()));
}

AutoPtr<CTypeStrings> CChoiceDataType::GetFullCType(void) const
{
    AutoPtr<CChoiceTypeStrings> code(new CChoiceTypeStrings(IdName(),
                                                            ClassName(),
                                                            Namespace()));
    bool haveUserClass = GetParentType() == 0;
    code->SetHaveUserClass(haveUserClass);
    code->SetObject(true);
    iterate ( TMembers, i, GetMembers() ) {
        code->AddVariant((*i)->GetName(), (*i)->GetType()->GetFullCType());
    }
    SetParentClassTo(*code);
    return AutoPtr<CTypeStrings>(code.release());
}

END_NCBI_SCOPE
