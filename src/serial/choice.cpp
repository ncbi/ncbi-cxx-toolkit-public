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

typedef CChoiceTypeInfoBase::TMemberIndex TMemberIndex;

CChoiceTypeInfoBase::CChoiceTypeInfoBase(const string& name)
    : CParent(name)
{
}

CChoiceTypeInfoBase::CChoiceTypeInfoBase(const char* name)
    : CParent(name)
{
}

CChoiceTypeInfoBase::~CChoiceTypeInfoBase(void)
{
}

void CChoiceTypeInfoBase::AddVariant(const CMemberId& id,
                                     const CTypeRef& typeRef)
{
    m_Members.AddMember(id, new CRealMemberInfo(0, typeRef.Get()));
}

void CChoiceTypeInfoBase::AddVariant(const string& name,
                                     const CTypeRef& typeRef)
{
    AddVariant(CMemberId(name), typeRef);
}

void CChoiceTypeInfoBase::AddVariant(const char* name,
                                     const CTypeRef& typeRef)
{
    AddVariant(CMemberId(name), typeRef);
}

TMemberIndex CChoiceTypeInfoBase::GetVariantsCount(void) const
{
    return m_Members.GetSize();
}

TTypeInfo CChoiceTypeInfoBase::GetVariantTypeInfo(TMemberIndex index) const
{
    return m_Members.GetMemberInfo(index)->GetTypeInfo();
}

bool CChoiceTypeInfoBase::IsDefault(TConstObjectPtr object) const
{
    return object == 0 || GetIndex(object) == -1;
}

void CChoiceTypeInfoBase::ResetIndex(TObjectPtr object) const
{
    SetIndex(object, -1);
}

void CChoiceTypeInfoBase::SetDelayIndex(TObjectPtr /*object*/,
                                          TMemberIndex /*index*/) const
{
    THROW1_TRACE(runtime_error, "illegal call");
}

bool CChoiceTypeInfoBase::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    TMemberIndex index = GetIndex(object1);
    if ( index != GetIndex(object2) )
        return false;
    if ( index == -1 )
        return true;
    if ( index >= 0 && index < GetVariantsCount() ) {
        return GetVariantTypeInfo(index)->Equals(GetData(object1, index),
                                                 GetData(object2, index));
    }
    return false;
}

void CChoiceTypeInfoBase::SetDefault(TObjectPtr dst) const
{
    ResetIndex(dst);
}

void CChoiceTypeInfoBase::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TMemberIndex index = GetIndex(src);
    if ( index >= 0 && index < GetVariantsCount() ) {
        SetIndex(dst, index);
        GetVariantTypeInfo(index)->Assign(GetData(dst, index),
                                          GetData(src, index));
    }
    else {
        ResetIndex(dst);
    }
}

void CChoiceTypeInfoBase::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    TMemberIndex index = GetIndex(object);
    if ( index >= 0 && index < GetVariantsCount() ) {
        CObjectOStream::Member m(out, GetMembers(), index);
        const CMemberInfo* memberInfo = m_Members.GetMemberInfo(index);
        if ( memberInfo->CanBeDelayed() &&
             memberInfo->GetDelayBuffer(object).Write(out) ) {
            // copied original delayed read buffer, do nothing
        }
        else {
            memberInfo->GetTypeInfo()->WriteData(out, GetData(object, index));
        }
    }
    else {
        THROW1_TRACE(runtime_error, "cannot write unset choice");
    }
}

void CChoiceTypeInfoBase::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    CObjectIStream::Member m(in, GetMembers());
    TMemberIndex index = m.GetIndex();
    const CMemberInfo* memberInfo = m_Members.GetMemberInfo(index);
    bool sameIndex = (index == GetIndex(object));
    if ( sameIndex ) {
        // already have some values set -> plain read even if
        // member can be delayed
    }
    else {
        // index is differnet from current -> first, reset choice
        ResetIndex(object);
        if ( memberInfo->CanBeDelayed() &&
             memberInfo->GetDelayBuffer(object).DelayRead(in,
                                                          object, index,
                                                          memberInfo) ) {
            // we've got delayed buffer -> select using delay buffer
            SetDelayIndex(object, index);
            return;
        }
        // select for reading
        SetIndex(object, index);
    }
    memberInfo->GetTypeInfo()->ReadData(in, GetData(object, index));
}

void CChoiceTypeInfoBase::SkipData(CObjectIStream& in) const
{
    CObjectIStream::Member m(in, GetMembers());
    TMemberIndex index = m.GetIndex();
    GetVariantTypeInfo(index)->SkipData(in);
}

bool CChoiceTypeInfoBase::MayContainType(TTypeInfo typeInfo) const
{
    return GetMembers().MayContainType(typeInfo);
}

bool CChoiceTypeInfoBase::HaveChildren(TConstObjectPtr object) const
{
    return GetIndex(object) >= 0;
}

void CChoiceTypeInfoBase::BeginTypes(CChildrenTypesIterator& cc) const
{
    GetMembers().BeginTypes(cc);
}

void CChoiceTypeInfoBase::Begin(CConstChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

void CChoiceTypeInfoBase::Begin(CChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = 0;
}

bool CChoiceTypeInfoBase::ValidTypes(const CChildrenTypesIterator& cc) const
{
    return GetMembers().ValidTypes(cc);
}

bool CChoiceTypeInfoBase::Valid(const CConstChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index == 0 && GetIndex(cc.GetParentPtr()) >= 0;
}

bool CChoiceTypeInfoBase::Valid(const CChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index == 0 && GetIndex(cc.GetParentPtr()) >= 0;
}

TTypeInfo CChoiceTypeInfoBase::GetChildType(const CChildrenTypesIterator& cc) const
{
    return GetMembers().GetChildType(cc);
}

void CChoiceTypeInfoBase::GetChild(const CConstChildrenIterator& cc,
                                CConstObjectInfo& child) const
{
    TMemberIndex index = GetIndex(cc.GetParentPtr());
    _ASSERT(index >= 0 && index < GetVariantsCount());
    child.Set(GetData(cc.GetParentPtr(), index), GetVariantTypeInfo(index));
}

void CChoiceTypeInfoBase::GetChild(const CChildrenIterator& cc,
                                CObjectInfo& child) const
{
    TMemberIndex index = GetIndex(cc.GetParentPtr());
    _ASSERT(index >= 0 && index < GetVariantsCount());
    child.Set(GetData(cc.GetParentPtr(), index), GetVariantTypeInfo(index));
}

void CChoiceTypeInfoBase::NextType(CChildrenTypesIterator& cc) const
{
    GetMembers().NextType(cc);
}

void CChoiceTypeInfoBase::Next(CConstChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CChoiceTypeInfoBase::Next(CChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CChoiceTypeInfoBase::Erase(CChildrenIterator& cc) const
{
    ResetIndex(cc.GetParentPtr());
}

CGeneratedChoiceInfo::CGeneratedChoiceInfo(const char* name,
                                           size_t size,
                                           TCreateFunction createFunc,
                                           TWhichFunction whichFunc,
                                           TResetFunction resetFunc,
                                           TSelectFunction selectFunc)
    : CParent(name), m_Size(size),
      m_CreateFunction(createFunc), m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc),
      m_SelectDelayFunction(0), m_PostReadFunction(0), m_PreWriteFunction(0)
{
}

void CGeneratedChoiceInfo::SetSelectDelay(TSelectDelayFunction func)
{
    _ASSERT(m_SelectDelayFunction == 0);
    _ASSERT(func != 0);
    m_SelectDelayFunction = func;
}

void CGeneratedChoiceInfo::SetPostRead(TPostReadFunction func)
{
    _ASSERT(m_PostReadFunction == 0);
    _ASSERT(func != 0);
    m_PostReadFunction = func;
}

void CGeneratedChoiceInfo::SetPreWrite(TPreWriteFunction func)
{
    _ASSERT(m_PreWriteFunction == 0);
    _ASSERT(func != 0);
    m_PreWriteFunction = func;
}

void DoSetPostRead(CGeneratedChoiceInfo* info,
                   void (*func)(TObjectPtr object))
{
    info->SetPostRead(func);
}

void DoSetPreWrite(CGeneratedChoiceInfo* info, void
                   (*func)(TConstObjectPtr object))
{
    info->SetPreWrite(func);
}

size_t CGeneratedChoiceInfo::GetSize(void) const
{
    return m_Size;
}

TObjectPtr CGeneratedChoiceInfo::Create(void) const
{
    return m_CreateFunction();
}

TMemberIndex CGeneratedChoiceInfo::GetIndex(TConstObjectPtr object) const
{
    return m_WhichFunction(object);
}

void CGeneratedChoiceInfo::ResetIndex(TObjectPtr object) const
{
    m_ResetFunction(object);
}

void CGeneratedChoiceInfo::SetIndex(TObjectPtr object,
                                    TMemberIndex index) const
{
    m_SelectFunction(object, index);
}

void CGeneratedChoiceInfo::SetDelayIndex(TObjectPtr object,
                                           TMemberIndex index) const
{
    m_SelectDelayFunction(object, index);
}

TObjectPtr CGeneratedChoiceInfo::x_GetData(TObjectPtr object,
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

void CGeneratedChoiceInfo::WriteData(CObjectOStream& out,
                                     TConstObjectPtr object) const
{
    if ( m_PreWriteFunction )
        m_PreWriteFunction(object);
    CParent::WriteData(out, object);
}

void CGeneratedChoiceInfo::ReadData(CObjectIStream& in,
                                    TObjectPtr object) const
{
    CParent::ReadData(in, object);
    if ( m_PostReadFunction )
        m_PostReadFunction(object);
}

END_NCBI_SCOPE
