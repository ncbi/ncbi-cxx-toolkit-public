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
#include <serial/iteratorbase.hpp>
#include <serial/bytesrc.hpp>
#include <serial/delaybuf.hpp>

BEGIN_NCBI_SCOPE

typedef CChoiceTypeInfo::TMemberIndex TMemberIndex;

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

bool CChoiceTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return object == 0 || GetIndex(object) == eEmptyIndex;
}

bool CChoiceTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    TMemberIndex index = GetIndex(object1);
    if ( index != GetIndex(object2) )
        return false;
    if ( index == eEmptyIndex )
        return true;
    if ( index >= 0 && index < GetMembersCount() ) {
        return GetMemberTypeInfo(index)->Equals(GetData(object1, index),
                                                GetData(object2, index));
    }
    return false;
}

void CChoiceTypeInfo::SetDefault(TObjectPtr dst) const
{
    ResetIndex(dst);
}

void CChoiceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TMemberIndex index = GetIndex(src);
    if ( index >= 0 && index < GetMembersCount() ) {
        SetIndex(dst, index);
        GetMemberTypeInfo(index)->Assign(GetData(dst, index),
                                         GetData(src, index));
    }
    else {
        ResetIndex(dst);
    }
}

void CChoiceTypeInfo::WriteData(CObjectOStream& out, TConstObjectPtr object) const
{
    DoPreWrite(object);
    TMemberIndex index = GetIndex(object);
    if ( index >= 0 && index < GetMembersCount() ) {
        const CMemberInfo* info = GetMemberInfo(index);
        const CMemberId& id = GetMemberId(index);
        if ( info->CanBeDelayed() ) {
             const CDelayBuffer& buffer = info->GetDelayBuffer(object);
             if ( buffer ) {
                 if ( buffer.HaveFormat(out.GetDataFormat()) ) {
                     out.WriteDelayedChoice(this, id, buffer);
                     return;
                 }
             }
        }

        out.WriteChoice(this, id,
                        info->GetTypeInfo(), GetData(object, index));
    }
    else {
        THROW1_TRACE(runtime_error, "cannot write unset choice");
    }
}

class CChoiceTypeInfoReader : public CObjectChoiceReader
{
public:
    CChoiceTypeInfoReader(const CChoiceTypeInfo* choice,
                          TObjectPtr object)
        : m_Choice(choice), m_Object(object)
        {
        }

    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CMembersInfo& members, int index)
        {
            const CChoiceTypeInfo* choice = m_Choice;
            TObjectPtr obj = m_Object;
            const CMemberInfo* m = members.GetMemberInfo(index);
            bool sameIndex = (index == choice->GetIndex(obj));
            if ( sameIndex ) {
                // already have some values set -> plain read even if
                // member can be delayed
            }
            else {
                // index is differnet from current -> first, reset choice
                choice->ResetIndex(obj);
                if ( m->CanBeDelayed() &&
                     m->GetDelayBuffer(obj).DelayRead(in, obj, index, m) ) {
                    // we've got delayed buffer -> select using delay buffer
                    choice->SetDelayIndex(obj, index);
                    return;
                }
                // select for reading
                choice->SetIndex(obj, index);
            }
            m->GetTypeInfo()->ReadData(in, choice->GetData(obj, index));
        }

private:
    const CChoiceTypeInfo* m_Choice;
    TObjectPtr m_Object;
};

class CChoiceTypeInfoSkipper : public CObjectChoiceReader
{
public:
    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CMembersInfo& members, int index)
        {
            members.GetMemberInfo(index)->GetTypeInfo()->SkipData(in);
        }
};

void CChoiceTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    CChoiceTypeInfoReader reader(this, object);
    in.ReadChoice(reader, this, GetMembers());
    DoPostRead(object);
}

void CChoiceTypeInfo::SkipData(CObjectIStream& in) const
{
    CChoiceTypeInfoSkipper skipper;
    in.ReadChoice(skipper, this, GetMembers());
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
        m_SelectFunction(object, eEmptyIndex);
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
    TObjectPtr memberPtr = info->GetMember(object);

    if ( info->CanBeDelayed() )
        info->GetDelayBuffer(object).Update();

    if ( info->IsPointer() ) {
        memberPtr = CType<TObjectPtr>::Get(memberPtr);
        _ASSERT(memberPtr != 0 );
    }
    return memberPtr;
}

bool CChoiceTypeInfo::HaveChildren(TConstObjectPtr object) const
{
    TTypeInfo parentClass = GetParentTypeInfo();
    if ( parentClass && parentClass->HaveChildren(object) )
        return true;
    return GetIndex(object) >= 0;
}

void CChoiceTypeInfo::Begin(CConstChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

void CChoiceTypeInfo::Begin(CChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

bool CChoiceTypeInfo::Valid(const CConstChildrenIterator& cc) const
{
    int index = cc.GetIndex().m_Index;
    return index == 0 && GetIndex(cc.GetParentPtr()) >= 0; // variant
}

bool CChoiceTypeInfo::Valid(const CChildrenIterator& cc) const
{
    int index = cc.GetIndex().m_Index;
    return index == 0 && GetIndex(cc.GetParentPtr()) >= 0; // variant
}

void CChoiceTypeInfo::GetChild(const CConstChildrenIterator& cc,
                               CConstObjectInfo& child) const
{
    _ASSERT(cc.GetIndex().m_Index == 0);
    TMemberIndex variant = GetIndex(cc.GetParentPtr());
    _ASSERT(variant >= 0 && variant < GetMembersCount());
    child.Set(GetData(cc.GetParentPtr(), variant), GetMemberTypeInfo(variant));
}

void CChoiceTypeInfo::GetChild(const CChildrenIterator& cc,
                                CObjectInfo& child) const
{
    _ASSERT(cc.GetIndex().m_Index == 0);
    TMemberIndex variant = GetIndex(cc.GetParentPtr());
    _ASSERT(variant >= 0 && variant < GetMembersCount());
    child.Set(GetData(cc.GetParentPtr(), variant), GetMemberTypeInfo(variant));
}

void CChoiceTypeInfo::Next(CConstChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CChoiceTypeInfo::Next(CChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CChoiceTypeInfo::Erase(CChildrenIterator& cc) const
{
    _ASSERT(cc.GetIndex().m_Index == 0);
    ResetIndex(cc.GetParentPtr());
}

END_NCBI_SCOPE
