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
* Revision 1.17  2000/06/07 19:45:57  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.16  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* Revision 1.15  2000/05/24 20:08:46  vasilche
* Implemented XML dump.
*
* Revision 1.14  2000/05/09 16:38:38  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.13  2000/03/14 14:42:29  vasilche
* Fixed error reporting.
*
* Revision 1.12  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.11  2000/02/17 20:02:42  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.10  2000/01/10 20:12:36  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.9  2000/01/10 19:46:39  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.8  2000/01/05 19:43:51  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.7  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.6  1999/11/16 15:40:19  vasilche
* Added plain pointer choice.
*
* Revision 1.5  1999/10/28 15:37:40  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.4  1999/10/25 19:07:15  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.3  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.2  1999/09/14 18:54:15  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:58  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <serial/choiceptr.hpp>
#include <serial/typeref.hpp>
#include <serial/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

CChoicePointerTypeInfo::CChoicePointerTypeInfo(TTypeInfo typeInfo)
    : CPointerTypeInfo(typeInfo)
{
    Init();
}

#ifdef TYPENAME
CChoicePointerTypeInfo::CChoicePointerTypeInfo(const string& name,
                                               TTypeInfo typeInfo)
    : CPointerTypeInfo(name, typeInfo)
{
    Init();
}

CChoicePointerTypeInfo::CChoicePointerTypeInfo(const char* name,
                                               TTypeInfo typeInfo)
    : CPointerTypeInfo(name, typeInfo)
{
    Init();
}
#endif

CChoicePointerTypeInfo::~CChoicePointerTypeInfo(void)
{
}

TTypeInfo CChoicePointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return new CChoicePointerTypeInfo(base);
}

TTypeInfo
CChoicePointerTypeInfo::GetRealDataTypeInfo(TConstObjectPtr object) const
{
    return GetVariantType(FindVariant(object));
}

TTypeInfo CChoicePointerTypeInfo::GetVariantType(TMemberIndex index) const
{
    return GetVariants().GetMemberInfo(index)->GetTypeInfo();
}

void CChoicePointerTypeInfo::Init(void)
{
    const CClassInfoTmpl::TSubClasses* subclasses =
        dynamic_cast<const CClassInfoTmpl&>(*GetDataTypeInfo()).SubClasses();
    if ( !subclasses )
        return;
    for ( CClassInfoTmpl::TSubClasses::const_iterator i = subclasses->begin();
          i != subclasses->end(); ++i ) {
        m_Variants.AddMember(i->first, 0, i->second.Get());
    }
}

const CChoicePointerTypeInfo::TVariantsByType&
CChoicePointerTypeInfo::VariantsByType(void) const
{    
    TVariantsByType* variants = m_VariantsByType.get();
    if ( !variants ) {
        m_VariantsByType.reset(variants = new TVariantsByType);

        for ( TIndex i = 0, size = GetVariants().GetSize();
              i != size; ++i ) {
            TTypeInfo type = GetVariantType(i);
            const type_info* id;
            if ( !type ) {
                // null variant
                id = &typeid(void);
                type = CNullTypeInfo::GetTypeInfo();
                _TRACE(GetDataTypeInfo()->GetName()
                       << ".AddSubClass: null");
            }
            else {
                id = &dynamic_cast<const CClassInfoTmpl&>(*type).GetId();
                _TRACE(GetDataTypeInfo()->GetName()
                       << ".AddSubClass: " << id->name());
            }
            if ( !variants->insert(TVariantsByType::
                                   value_type(id, i)).second ) {
                THROW1_TRACE(runtime_error,
                             "conflict subclasses: " + type->GetName());
            }
        }
    }
    return *variants;
}

CChoicePointerTypeInfo::TIndex
CChoicePointerTypeInfo::FindVariant(TConstObjectPtr object) const
{    
    const TVariantsByType& variants = VariantsByType();
    const type_info* id;
    if ( object == 0 ) {
        // null object: find null subclass
        id = &typeid(void);
    }
    else {
        id = dynamic_cast<const CClassInfoTmpl*>(GetDataTypeInfo())->
            GetCPlusPlusTypeInfo(object);
    }
    TVariantsByType::const_iterator p = variants.find(id);
    if ( p == variants.end() )
        THROW1_TRACE(runtime_error, "incompatible CChoicePointer type");
    return p->second;
}

void CChoicePointerTypeInfo::WriteData(CObjectOStream& out,
                                       TConstObjectPtr object) const
{
    TConstObjectPtr data = GetObjectPointer(object);
    TIndex index = FindVariant(data);
    const CMemberId& id = GetVariants().GetMemberId(index);
    TTypeInfo dataType = GetVariantType(index);
    if ( !dataType )
        dataType = CNullTypeInfo::GetTypeInfo();
    out.WriteChoice(this, id, dataType, data);
}

class CChoicePointerTypeInfoReader : public CObjectChoiceReader
{
public:
    CChoicePointerTypeInfoReader(const CChoicePointerTypeInfo* choice,
                                 TObjectPtr object)
        : m_Choice(choice), m_Object(object)
        {
        }

    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CMembersInfo& members, int index)
        {
            const CChoicePointerTypeInfo* choice = m_Choice;
            TObjectPtr object = m_Object;
            TTypeInfo dataType = members.GetMemberInfo(index)->GetTypeInfo();
            if ( !dataType )
                dataType = CNullTypeInfo::GetTypeInfo();
            TObjectPtr data = dataType->Create();
            choice->SetObjectPointer(object, data);
            in.ReadExternalObject(data, dataType);
        }

private:
    const CChoicePointerTypeInfo* m_Choice;
    TObjectPtr m_Object;
};

class CChoicePointerTypeInfoSkipper : public CObjectChoiceReader
{
public:
    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CMembersInfo& members, int index)
        {
            TTypeInfo dataType = members.GetMemberInfo(index)->GetTypeInfo();
            if ( !dataType )
                dataType = CNullTypeInfo::GetTypeInfo();
            in.SkipExternalObject(dataType);
        }
};

void CChoicePointerTypeInfo::ReadData(CObjectIStream& in,
                                      TObjectPtr object) const
{
    CChoicePointerTypeInfoReader reader(this, object);
    in.ReadChoice(reader, this, GetVariants());
}

void CChoicePointerTypeInfo::SkipData(CObjectIStream& in) const
{
    CChoicePointerTypeInfoSkipper skipper;
    in.ReadChoice(skipper, this, GetVariants());
}

CNullTypeInfo::CNullTypeInfo(void)
{
}

CNullTypeInfo::~CNullTypeInfo(void)
{
}

TObjectPtr CNullTypeInfo::Create(void) const
{
    return 0;
}

TTypeInfo CNullTypeInfo::GetTypeInfo(void)
{
    TTypeInfo typeInfo = new CNullTypeInfo();
    return typeInfo;
}

void CNullTypeInfo::WriteData(CObjectOStream& out,
                              TConstObjectPtr object) const
{
    if ( object != 0 ) {
        THROW1_TRACE(runtime_error, "non null value");
    }
    out.WriteNull();
}

void CNullTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    if ( object != 0 ) {
        THROW1_TRACE(runtime_error, "non null value");
    }
    in.ReadNull();
}

void CNullTypeInfo::SkipData(CObjectIStream& in) const
{
    in.SkipNull();
}

END_NCBI_SCOPE
