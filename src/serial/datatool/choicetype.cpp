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
* Revision 1.31  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.30  2005/02/02 19:08:36  gouriano
* Corrected DTD generation
*
* Revision 1.29  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.28  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.27  2002/12/26 19:29:47  gouriano
* corrected CreateTypeInfo method to handle XML attribute lists
*
* Revision 1.26  2002/11/19 19:48:28  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.25  2002/11/14 21:03:39  gouriano
* added support of XML attribute lists
*
* Revision 1.24  2002/10/15 13:58:04  gouriano
* use "noprefix" flag
*
* Revision 1.23  2001/12/03 14:51:29  juran
* Heed warning.
*
* Revision 1.22  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.21  2000/11/20 17:26:32  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.20  2000/11/08 17:02:50  vasilche
* Added generation of modular DTD files.
*
* Revision 1.19  2000/11/07 17:26:25  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
* Added possibility to set include directory for whole module
*
* Revision 1.18  2000/10/03 17:22:49  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.17  2000/09/26 17:38:25  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.16  2000/09/18 20:00:28  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.15  2000/08/25 15:59:20  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.14  2000/08/15 19:45:28  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.13  2000/06/16 16:31:38  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.12  2000/05/24 20:09:28  vasilche
* Implemented DTD generation.
*
* Revision 1.11  2000/05/03 14:38:17  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.10  2000/04/17 19:11:07  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.9  2000/04/12 15:36:49  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
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

#include <ncbi_pch.hpp>
#include <serial/datatool/choicetype.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/choice.hpp>
#include <serial/datatool/value.hpp>
#include <serial/datatool/choicestr.hpp>
#include <serial/datatool/choiceptrstr.hpp>
#include <serial/member.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

struct CAnyTypeChoice {
    AnyType data;
    TMemberIndex index;
    CAnyTypeChoice(void)
        : index(kEmptyChoice)
        {
        }
};

static TObjectPtr CreateAnyTypeChoice(TTypeInfo /*typeInfo*/,
                                      CObjectMemoryPool* /*memoryPool*/)
{
    return new CAnyTypeChoice();
}

static
TMemberIndex GetIndexAnyTypeChoice(const CChoiceTypeInfo* /*choiceType*/,
                                   TConstObjectPtr choicePtr)
{
    const CAnyTypeChoice* choice =
        static_cast<const CAnyTypeChoice*>(choicePtr);
    return choice->index;
}

static
void SetIndexAnyTypeChoice(const CChoiceTypeInfo* /*choiceType*/,
                           TObjectPtr choicePtr,
                           TMemberIndex index,
                           CObjectMemoryPool* /*memoryPool*/)
{
    CAnyTypeChoice* choice = static_cast<CAnyTypeChoice*>(choicePtr);
    choice->index = index;
}

static
void ResetIndexAnyTypeChoice(const CChoiceTypeInfo* /*choiceType*/,
                             TObjectPtr choicePtr)
{
    CAnyTypeChoice* choice = static_cast<CAnyTypeChoice*>(choicePtr);
    choice->index = kEmptyChoice;
}

const char* CChoiceDataType::GetASNKeyword(void) const
{
    return "CHOICE";
}

void CChoiceDataType::FixTypeTree(void) const
{
    CParent::FixTypeTree();
    ITERATE ( TMembers, m, GetMembers() ) {
        (*m)->GetType()->SetInChoice(this);
    }
}

const char* CChoiceDataType::XmlMemberSeparator(void) const
{
    return " | ";
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
    auto_ptr<CChoiceTypeInfo>
        typeInfo(new CChoiceTypeInfo(sizeof(CAnyTypeChoice), GlobalName(),
                                     TObjectPtr(0), &CreateAnyTypeChoice,
                                     typeid(CAnyTypeChoice),
                                     &GetIndexAnyTypeChoice,
                                     &SetIndexAnyTypeChoice,
                                     &ResetIndexAnyTypeChoice));
    for ( TMembers::const_iterator i = GetMembers().begin();
          i != GetMembers().end(); ++i ) {
        CDataMember* member = i->get();
        if (member->Attlist()) {
            CMemberInfo* memInfo =
                typeInfo->AddMember(member->GetName(),0,
                                    member->GetType()->GetTypeInfo());
            if (member->NoPrefix()) {
                memInfo->SetNoPrefix();
            }
            if (member->Attlist()) {
                memInfo->SetAttlist();
            }
            if (member->Notag()) {
                memInfo->SetNotag();
            }
        } else {
            CVariantInfo* varInfo = 
                typeInfo->AddVariant(member->GetName(), 0,
                                     member->GetType()->GetTypeInfo());
            if (member->NoPrefix()) {
                varInfo->SetNoPrefix();
            }
            if (member->Notag()) {
                varInfo->SetNotag();
            }
        }
    }
    return UpdateModuleName(typeInfo.release());
}

AutoPtr<CTypeStrings> CChoiceDataType::GenerateCode(void) const
{
    return GetFullCType();
}

AutoPtr<CTypeStrings> CChoiceDataType::GetRefCType(void) const
{
    if ( GetBoolVar("_virtual_choice") ) {
        AutoPtr<CTypeStrings> cls(new CClassRefTypeStrings(ClassName(),
                                                           Namespace(),
                                                           FileName()));
        return AutoPtr<CTypeStrings>(new CChoicePtrRefTypeStrings(cls));
    }
    else {
        return AutoPtr<CTypeStrings>(new CChoiceRefTypeStrings(ClassName(),
                                                               Namespace(),
                                                               FileName()));
    }
}

AutoPtr<CTypeStrings> CChoiceDataType::GetFullCType(void) const
{
    if ( GetBoolVar("_virtual_choice") ) {
        AutoPtr<CChoicePtrTypeStrings>
            code(new CChoicePtrTypeStrings(GlobalName(),
                                           ClassName()));
        ITERATE ( TMembers, i, GetMembers() ) {
            AutoPtr<CTypeStrings> varType = (*i)->GetType()->GetFullCType();
            code->AddVariant((*i)->GetName(), varType);
        }
        SetParentClassTo(*code);
        return AutoPtr<CTypeStrings>(code.release());
    }
    else {
        bool rootClass = GetParentType() == 0;
        AutoPtr<CChoiceTypeStrings> code(new CChoiceTypeStrings(GlobalName(),
                                                                ClassName()));
        bool haveUserClass = rootClass;
        code->SetHaveUserClass(haveUserClass);
        code->SetObject(true);
        ITERATE ( TMembers, i, GetMembers() ) {
            AutoPtr<CTypeStrings> varType = (*i)->GetType()->GetFullCType();
            bool delayed = GetBoolVar((*i)->GetName()+"._delay");
            bool in_union = GetBoolVar((*i)->GetName()+"._in_union", true);
            code->AddVariant((*i)->GetName(), varType, delayed, in_union,
                             (*i)->GetType()->GetTag(),
                             (*i)->NoPrefix(), (*i)->Attlist(), (*i)->Notag(),
                             (*i)->SimpleType(),(*i)->GetType());
            (*i)->GetType()->SetTypeStr(&(*code));
        }
        SetTypeStr(&(*code));
        SetParentClassTo(*code);
        return AutoPtr<CTypeStrings>(code.release());
    }
}

END_NCBI_SCOPE
