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
* Revision 1.19  2000/08/15 19:44:46  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.18  2000/07/03 18:42:42  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.17  2000/06/16 16:31:17  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.16  2000/06/07 19:45:57  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.15  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* Revision 1.14  2000/05/24 20:08:46  vasilche
* Implemented XML dump.
*
* Revision 1.13  2000/05/09 16:38:37  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.12  2000/04/28 16:58:12  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.11  2000/04/10 21:01:48  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.10  2000/04/06 16:10:58  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.9  2000/04/03 18:47:26  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.8  2000/03/14 14:42:29  vasilche
* Fixed error reporting.
*
* Revision 1.7  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.6  2000/02/17 20:02:42  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.5  2000/02/01 21:47:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.4  2000/01/10 19:46:38  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.3  2000/01/05 19:43:51  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.2  1999/12/17 19:05:01  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.1  1999/09/24 18:20:07  vasilche
* Removed dependency on NCBI toolkit.
*
* 
* ===========================================================================
*/

#include <serial/choice.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/memberid.hpp>
#include <serial/member.hpp>
#include <serial/bytesrc.hpp>
#include <serial/delaybuf.hpp>

BEGIN_NCBI_SCOPE

CChoiceTypeInfo::CChoiceTypeInfo(const char* name,
                                 size_t size,
                                 const type_info& ti,
                                 TCreateFunction createFunc,
                                 TWhichFunction whichFunc,
                                 TSelectFunction selectFunc,
                                 TResetFunction resetFunc)
    : CParent(name, ti, size),
      m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc),
      m_SelectDelayFunction(0)
{
    SetCreateFunction(createFunc);
}

CChoiceTypeInfo::CChoiceTypeInfo(const string& name,
                                 size_t size,
                                 const type_info& ti,
                                 TCreateFunction createFunc,
                                 TWhichFunction whichFunc,
                                 TSelectFunction selectFunc,
                                 TResetFunction resetFunc)
    : CParent(name, ti, size),
      m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc),
      m_SelectDelayFunction(0)
{
    SetCreateFunction(createFunc);
}

CTypeInfo::ETypeFamily CChoiceTypeInfo::GetTypeFamily(void) const
{
    return eTypeChoice;
}

bool CChoiceTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return object == 0 || GetIndex(object) == kEmptyChoice;
}

bool CChoiceTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    TMemberIndex index = GetIndex(object1);
    if ( index != GetIndex(object2) )
        return false;
    if ( index == kEmptyChoice )
        return true;
    return
        GetMemberInfo(index)->GetTypeInfo()->Equals(GetData(object1, index),
                                                    GetData(object2, index));
}

void CChoiceTypeInfo::SetDefault(TObjectPtr dst) const
{
    ResetIndex(dst);
}

void CChoiceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TMemberIndex index = GetIndex(src);
    if ( index == kEmptyChoice )
        ResetIndex(dst);
    else {
        _ASSERT(index >= GetMembers().FirstMemberIndex() && 
                index <= GetMembers().LastMemberIndex());
        SetIndex(dst, index);
        GetMemberInfo(index)->GetTypeInfo()->Assign(GetData(dst, index),
                                                    GetData(src, index));
    }
}

void CChoiceTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    in.ReadChoice(CObjectInfo(object, this,
                              CObjectInfo::eNonCObject));
    DoPostRead(object);
}

void CChoiceTypeInfo::SkipData(CObjectIStream& in) const
{
    in.SkipChoice(this);
}

void CChoiceTypeInfo::WriteData(CObjectOStream& out,
                                TConstObjectPtr object) const
{
    DoPreWrite(object);
    out.WriteChoice(CConstObjectInfo(object, this,
                                     CObjectInfo::eNonCObject));
}

void CChoiceTypeInfo::SetSelectDelayFunction(TSelectDelayFunction func)
{
    _ASSERT(m_SelectDelayFunction == 0);
    _ASSERT(func != 0);
    m_SelectDelayFunction = func;
}

TMemberIndex CChoiceTypeInfo::GetIndex(TConstObjectPtr object) const
{
    return m_WhichFunction(object);
}

void CChoiceTypeInfo::ResetIndex(TObjectPtr object) const
{
    if ( m_ResetFunction )
        m_ResetFunction(object);
    else
        m_SelectFunction(object, kEmptyChoice);
}

void CChoiceTypeInfo::SetIndex(TObjectPtr object,
                               TMemberIndex index) const
{
    m_SelectFunction(object, index);
}

void CChoiceTypeInfo::SetDelayIndex(TObjectPtr object,
                                    TMemberIndex index) const
{
    m_SelectDelayFunction(object, index);
}

TObjectPtr CChoiceTypeInfo::x_GetData(TObjectPtr object,
                                      TMemberIndex index) const
{
    _ASSERT(object != 0);
    _ASSERT(GetIndex(object) == index);
    const CMemberInfo* info = GetMembers().GetMemberInfo(index);
    if ( info->CanBeDelayed() )
        info->GetDelayBuffer(object).Update();

    TObjectPtr memberPtr = info->GetMember(object);
    if ( info->IsPointer() ) {
        memberPtr = CType<TObjectPtr>::Get(memberPtr);
        _ASSERT(memberPtr != 0 );
    }
    return memberPtr;
}

END_NCBI_SCOPE
