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
* Revision 1.47  2000/06/16 16:31:18  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.46  2000/06/07 19:45:57  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.45  2000/06/01 19:07:02  vasilche
* Added parsing of XML data.
*
* Revision 1.44  2000/05/24 20:08:46  vasilche
* Implemented XML dump.
*
* Revision 1.43  2000/05/09 16:38:38  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.42  2000/05/03 14:38:13  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.41  2000/04/10 21:01:48  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.40  2000/04/10 19:32:52  vakatov
* Get rid of a minor compiler warning
*
* Revision 1.39  2000/04/10 18:01:56  vasilche
* Added Erase() for STL types in type iterators.
*
* Revision 1.38  2000/04/06 16:10:59  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.37  2000/04/03 18:47:26  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.36  2000/03/29 21:54:48  vasilche
* Fixed internal compiler error on MSVC.
*
* Revision 1.35  2000/03/29 15:55:26  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.34  2000/03/14 14:42:30  vasilche
* Fixed error reporting.
*
* Revision 1.33  2000/03/10 15:00:35  vasilche
* Fixed OPTIONAL members reading.
*
* Revision 1.32  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.31  2000/02/17 20:02:43  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.30  2000/02/01 21:47:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.29  2000/01/10 19:46:39  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.28  2000/01/05 19:43:52  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.27  1999/12/28 18:55:49  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.26  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.25  1999/11/19 18:26:00  vasilche
* Fixed conflict with SetImplicit() in generated choice variant classes.
*
* Revision 1.24  1999/10/05 14:08:33  vasilche
* Fixed class name under GCC and MSVC.
*
* Revision 1.23  1999/10/04 16:22:16  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.22  1999/09/22 20:11:54  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.21  1999/09/14 18:54:16  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.20  1999/09/01 17:38:12  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.19  1999/08/31 17:50:08  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.18  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.17  1999/07/20 18:23:09  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.16  1999/07/13 20:18:17  vasilche
* Changed types naming.
*
* Revision 1.15  1999/07/07 19:59:03  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.14  1999/07/02 21:31:52  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.13  1999/07/01 17:55:26  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.12  1999/06/30 18:54:59  vasilche
* Fixed some errors under MSVS
*
* Revision 1.11  1999/06/30 16:04:48  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.10  1999/06/24 14:44:53  vasilche
* Added binary ASN.1 output.
*
* Revision 1.9  1999/06/17 18:38:52  vasilche
* Fixed order of members in class.
* Added checks for overlapped members.
*
* Revision 1.8  1999/06/16 20:35:29  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.7  1999/06/15 16:19:47  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.6  1999/06/10 21:06:45  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.5  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.4  1999/06/07 19:59:40  vasilche
* offset_t -> size_t
*
* Revision 1.3  1999/06/07 19:30:24  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:44  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:51  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/iteratorbase.hpp>
#include <serial/delaybuf.hpp>
#include <serial/stdtypes.hpp>

BEGIN_NCBI_SCOPE

CClassTypeInfo::CClassTypeInfo(const type_info& ti, size_t size)
    : CParent(ti, size), m_RandomOrder(false), m_Implicit(false),
      m_ParentClassInfo(0), m_GetTypeIdFunction(0)
{
}

CClassTypeInfo::CClassTypeInfo(const string& name, const type_info& ti, size_t size)
    : CParent(name, ti, size), m_RandomOrder(false), m_Implicit(false),
      m_ParentClassInfo(0), m_GetTypeIdFunction(0)
{
}

CClassTypeInfo::CClassTypeInfo(const char* name, const type_info& ti, size_t size)
    : CParent(name, ti, size), m_RandomOrder(false), m_Implicit(false),
      m_ParentClassInfo(0), m_GetTypeIdFunction(0)
{
}

void CClassTypeInfo::AddSubClass(const CMemberId& id,
                                 const CTypeRef& type)
{
    TSubClasses* subclasses = m_SubClasses.get();
    if ( !subclasses )
        m_SubClasses.reset(subclasses = new TSubClasses);
    subclasses->push_back(make_pair(id, type));
}

void CClassTypeInfo::AddSubClassNull(const CMemberId& id)
{
    AddSubClass(id, CTypeRef(TTypeInfo(0)));
}

void CClassTypeInfo::AddSubClass(const char* id, TTypeInfoGetter getter)
{
    AddSubClass(CMemberId(id), getter);
}

void CClassTypeInfo::AddSubClassNull(const char* id)
{
    AddSubClassNull(CMemberId(id));
}

TTypeInfo CClassTypeInfo::GetParentTypeInfo(void) const
{
    return m_ParentClassInfo;
}

void CClassTypeInfo::SetParentClass(TTypeInfo parentType)
{
    const CClassTypeInfo* parentClass =
        dynamic_cast<const CClassTypeInfo*>(parentType);
    _ASSERT(parentClass != 0);
    _ASSERT(IsCObject() == parentClass->IsCObject());
    _ASSERT(!m_ParentClassInfo);
    m_ParentClassInfo = parentClass;
}

void CClassTypeInfo::SetGetTypeIdFunction(TGetTypeIdFunction func)
{
    _ASSERT(m_GetTypeIdFunction == 0);
    _ASSERT(func != 0);
    m_GetTypeIdFunction = func;
}

const type_info* CClassTypeInfo::GetCPlusPlusTypeInfo(TConstObjectPtr object) const
{
    if ( m_GetTypeIdFunction ) 
        return m_GetTypeIdFunction(object);
    return 0;
}

TTypeInfo CClassTypeInfo::GetRealTypeInfo(TConstObjectPtr object) const
{
    const type_info* ti = GetCPlusPlusTypeInfo(object);
    if ( ti == 0 || ti == &GetId() )
        return this;
    RegisterSubClasses();
    return GetClassInfoById(*ti);
}

void CClassTypeInfo::RegisterSubClasses(void) const
{
    const TSubClasses* subclasses = m_SubClasses.get();
    if ( subclasses ) {
        for ( TSubClasses::const_iterator i = subclasses->begin();
              i != subclasses->end();
              ++i ) {
            const CClassTypeInfo* classInfo = 
                dynamic_cast<const CClassTypeInfo*>(i->second.Get());
            if ( classInfo )
                classInfo->RegisterSubClasses();
        }
    }
}

static inline
void CheckMemberOptional(CObjectIStream& in,
                         const CMembersInfo& members, size_t index)
{
    const CMemberInfo* info = members.GetMemberInfo(index);
    if ( !info->Optional() ) {
        in.ThrowError(in.eFormatError,
                      "member "+members.GetMemberId(index).ToString()+
                      " expected");
    }
}

static inline
TObjectPtr GetMember(const CMemberInfo* memberInfo, TObjectPtr object)
{
    if ( memberInfo->CanBeDelayed() )
        memberInfo->GetDelayBuffer(object).Update();
    return memberInfo->GetMember(object);
}

static inline
TConstObjectPtr GetMember(const CMemberInfo* memberInfo,
                          TConstObjectPtr object)
{
    if ( memberInfo->CanBeDelayed() )
        const_cast<CDelayBuffer&>(memberInfo->GetDelayBuffer(object)).Update();
    return memberInfo->GetMember(object);
}

static
void AssignMemberDefault(TObjectPtr object, const CMemberInfo* info)
{
    // check 'set' flag
    bool haveSetFlag = info->HaveSetFlag();
    if ( haveSetFlag && !info->GetSetFlag(object) )
        return; // member not set

    TObjectPtr member = GetMember(info, object);
    // assign member dafault
    TTypeInfo memberType = info->GetTypeInfo();
    TConstObjectPtr def = info->GetDefault();
    if ( def == 0 ) {
        if ( !memberType->IsDefault(member) )
            memberType->SetDefault(member);
    }
    else {
        memberType->Assign(member, def);
    }
    // update 'set' flag
    if ( haveSetFlag )
        info->GetSetFlag(object) = false;
}

static inline
void AssignMemberDefault(TObjectPtr object,
                         const CMembersInfo& members, size_t index)
{
    AssignMemberDefault(object, members.GetMemberInfo(index));
}

inline
const CMemberInfo* GetImplicitMember(const CMembersInfo& members)
{
    _ASSERT(members.GetMembersCount() == 1);
    return members.GetMemberInfo(0);
}

class CClassInfoClassWriter : public CObjectClassWriter
{
public:
    CClassInfoClassWriter(TConstObjectPtr object)
        : m_Object(object)
        {
        }

    virtual void WriteParentClass(CObjectOStream& out, TTypeInfo parentClass)
        {
            parentClass->WriteData(out, m_Object);
        }

    virtual void WriteMembers(CObjectOStream& out,
                              const CMembersInfo& members)
        {
            TConstObjectPtr obj = m_Object;
        
            size_t memberCount = members.GetMembersCount();
            for ( size_t index = 0; index < memberCount; ++index ) {
                const CMemberInfo* info = members.GetMemberInfo(index);
                bool haveSetFlag = info->HaveSetFlag();
                if ( haveSetFlag && !info->GetSetFlag(obj) )
                    // 'not set'
                    continue;
                
                if ( info->CanBeDelayed() ) {
                     const CDelayBuffer& buffer = info->GetDelayBuffer(obj);
                     if ( buffer ) {
                         if ( buffer.HaveFormat(out.GetDataFormat()) ) {
                             out.WriteDelayedClassMember(*this,
                                 members.GetMemberId(index), buffer);
                             continue;
                         }
                         // cannot write delayed buffer -> proceed after update
                         const_cast<CDelayBuffer&>(buffer).Update();
                     }
                }

                TConstObjectPtr member = info->GetMember(obj);
                TTypeInfo typeInfo = info->GetTypeInfo();
                if ( !haveSetFlag && info->Optional() ) {
                    TConstObjectPtr def = info->GetDefault();
                    if ( !def ) {
                        if ( typeInfo->IsDefault(member) )
                            continue; // default
                    }
                    else {
                        if ( typeInfo->Equals(member, def) )
                            continue; // default
                    }
                }

                out.WriteClassMember(*this, members.GetMemberId(index),
                                     typeInfo, member);
            }
        }

private:
    TConstObjectPtr m_Object;
};

class CClassInfoClassReader : public CObjectClassReader
{
public:
    CClassInfoClassReader(TObjectPtr object)
        : m_Object(object)
        {
        }
    virtual void ReadParentClass(CObjectIStream& in, TTypeInfo parentClassInfo)
        {
            parentClassInfo->ReadData(in, m_Object);
        }
    virtual void ReadMember(CObjectIStream& in,
                            const CMembersInfo& members, int index)
        {
            const CMemberInfo* info = members.GetMemberInfo(index);
            TObjectPtr obj = m_Object;
            if ( info->CanBeDelayed() &&
                 info->GetDelayBuffer(obj).DelayRead(in, obj,
                                                     CDelayBuffer::eNoMemberIndex,
                                                     info) ) {
                // we've got delayed buffer
            }
            else {
                // read member data
                info->GetTypeInfo()->ReadData(in, info->GetMember(obj));
            }
            // update 'set' flag
            if ( info->HaveSetFlag() )
                info->GetSetFlag(obj) = true;
        }
    virtual void AssignMemberDefault(CObjectIStream& in,
                                     const CMembersInfo& members, int index)
        {
            const CMemberInfo* info = members.GetMemberInfo(index);
            if ( !info->Optional() )
                CObjectClassReader::AssignMemberDefault(in, members, index);

            TObjectPtr obj = m_Object;
            bool haveSetFlag = info->HaveSetFlag();
            if ( haveSetFlag && !info->GetSetFlag(obj) )
                return; // member not set
            
            TObjectPtr member = GetMember(info, obj);
            // assign member dafault
            TTypeInfo memberType = info->GetTypeInfo();
            TConstObjectPtr def = info->GetDefault();
            if ( def == 0 ) {
                if ( !memberType->IsDefault(member) )
                    memberType->SetDefault(member);
            }
            else {
                memberType->Assign(member, def);
            }
            // update 'set' flag
            if ( haveSetFlag )
                info->GetSetFlag(obj) = false;
        }
private:
    TObjectPtr m_Object;
};

class CClassInfoClassSkipper : public CObjectClassReader
{
public:
    virtual void ReadParentClass(CObjectIStream& in, TTypeInfo parentClassInfo)
        {
            parentClassInfo->SkipData(in);
        }
    virtual void ReadMember(CObjectIStream& in,
                            const CMembersInfo& members, int index)
        {
            members.GetMemberInfo(index)->GetTypeInfo()->SkipData(in);
        }
    virtual void AssignMemberDefault(CObjectIStream& in,
                                     const CMembersInfo& members, int index)
        {
            if ( !members.GetMemberInfo(index)->Optional() )
                CObjectClassReader::AssignMemberDefault(in, members, index);
        }
};

void CClassTypeInfo::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    DoPreWrite(object);
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info = GetImplicitMember(GetMembers());
        out.WriteNamedType(this, info->GetTypeInfo(), GetMember(info, object));
    }
    else {
        CClassInfoClassWriter writer(object);
        out.WriteClass(writer, this, GetMembers(), RandomOrder());
    }
}

void CClassTypeInfo::SkipData(CObjectIStream& in) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info = GetImplicitMember(GetMembers());
        in.SkipNamedType(this, info->GetTypeInfo());
    }
    else {
        CClassInfoClassSkipper skipper;
        in.ReadClass(skipper, this, GetMembers(), RandomOrder());
    }
}

void CClassTypeInfo::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info = GetImplicitMember(GetMembers());
        in.ReadNamedType(this, info->GetTypeInfo(), GetMember(info, object));
    }
    else {
        CClassInfoClassReader reader(object);
        in.ReadClass(reader, this, GetMembers(), RandomOrder());
    }
    DoPostRead(object);
}

bool CClassTypeInfo::IsDefault(TConstObjectPtr /*object*/) const
{
    return false;
}

void CClassTypeInfo::SetDefault(TObjectPtr dst) const
{
    for ( TMemberIndex i = 0, size = GetMembersCount(); i < size; ++i ) {
        AssignMemberDefault(dst, GetMembers(), i);
    }
}

bool CClassTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
{
    for ( TMemberIndex i = 0, size = GetMembersCount(); i < size; ++i ) {
        const CMemberInfo* info = GetMemberInfo(i);
        if ( !info->GetTypeInfo()->Equals(GetMember(info, object1),
                                          GetMember(info, object2)) )
            return false;
        if ( info->HaveSetFlag() ) {
            if ( info->GetSetFlag(object1) != info->GetSetFlag(object2) )
                return false;
        }
    }
    return true;
}

void CClassTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    for ( TMemberIndex i = 0, size = GetMembersCount(); i < size; ++i ) {
        const CMemberInfo* info = GetMemberInfo(i);
        info->GetTypeInfo()->Assign(GetMember(info, dst),
                                    GetMember(info, src));
        if ( info->HaveSetFlag() ) {
            info->GetSetFlag(dst) = info->GetSetFlag(src);
        }
    }
}

bool CClassTypeInfo::IsType(TTypeInfo typeInfo) const
{
    return typeInfo->IsParentClassOf(this);
}

bool CClassTypeInfo::IsParentClassOf(const CClassTypeInfo* typeInfo) const
{
    for ( ; typeInfo; typeInfo = typeInfo->m_ParentClassInfo ) {
        if ( typeInfo == this )
            return true;
    }
    return false;
}

void CClassTypeInfo::BeginTypes(CChildrenTypesIterator& cc) const
{
    if ( m_ParentClassInfo )
        cc.GetIndex().m_Index = eIteratorIndexParentClass;
    else
        CParent::BeginTypes(cc);
}

TTypeInfo CClassTypeInfo::GetChildType(const CChildrenTypesIterator& cc) const
{
    if ( cc.GetIndex().m_Index == eIteratorIndexParentClass ) {
        _ASSERT(m_ParentClassInfo);
        return m_ParentClassInfo;
    }
    else
        return CParent::GetChildType(cc);
}

bool CClassTypeInfo::CalcMayContainType(TTypeInfo typeInfo) const
{
    const CClassTypeInfoBase* parentClass = m_ParentClassInfo;
    return parentClass && parentClass->MayContainType(typeInfo) ||
        CParent::CalcMayContainType(typeInfo);
}

bool CClassTypeInfo::HaveChildren(TConstObjectPtr object) const
{
    TTypeInfo parentClassInfo = GetParentTypeInfo();
    if ( parentClassInfo && parentClassInfo->HaveChildren(object) )
        return true;
    return !GetMembers().Empty();
}

void CClassTypeInfo::Begin(CConstChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = m_ParentClassInfo? eIteratorIndexParentClass: 0;
}

void CClassTypeInfo::Begin(CChildrenIterator& cc) const
{
    cc.GetIndex().m_Index = m_ParentClassInfo? eIteratorIndexParentClass: 0;
}

bool CClassTypeInfo::Valid(const CConstChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index < GetMembersCount();
}

bool CClassTypeInfo::Valid(const CChildrenIterator& cc) const
{
    return cc.GetIndex().m_Index < GetMembersCount();
}

void CClassTypeInfo::GetChild(const CConstChildrenIterator& cc,
                              CConstObjectInfo& child) const
{
    int index = cc.GetIndex().m_Index;
    if ( index == eIteratorIndexParentClass ) {
        _ASSERT(GetParentTypeInfo());
        child.Set(cc.GetParentPtr(), GetParentTypeInfo());
    }
    else {
        const CMemberInfo* member = GetMemberInfo(index);
        child.Set(member->GetMember(cc.GetParentPtr()), member->GetTypeInfo());
    }
}

void CClassTypeInfo::GetChild(const CChildrenIterator& cc,
                              CObjectInfo& child) const
{
    int index = cc.GetIndex().m_Index;
    if ( index == eIteratorIndexParentClass ) {
        _ASSERT(GetParentTypeInfo());
        child.Set(cc.GetParentPtr(), GetParentTypeInfo());
    }
    else {
        const CMemberInfo* member = GetMemberInfo(index);
        child.Set(member->GetMember(cc.GetParentPtr()), member->GetTypeInfo());
    }
}

void CClassTypeInfo::Next(CConstChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CClassTypeInfo::Next(CChildrenIterator& cc) const
{
    ++cc.GetIndex().m_Index;
}

void CClassTypeInfo::Erase(CChildrenIterator& cc) const
{
    int index = cc.GetIndex().m_Index;
    if ( index == eIteratorIndexParentClass )
        THROW1_TRACE(runtime_error, "cannot erase parent class");

    const CMemberInfo* info = GetMemberInfo(cc.GetIndex().m_Index);
    if ( info->GetDefault() )
        THROW1_TRACE(runtime_error, "cannot erase member with default value");

    AssignMemberDefault(cc.GetParentPtr(), info);
}

class CCObjectClassInfo : public CStdTypeInfo<void>
{
    typedef CTypeInfo CParent;
public:
    virtual bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
};

TTypeInfo CObjectGetTypeInfo::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCObjectClassInfo;
    return typeInfo;
}

bool CCObjectClassInfo::IsParentClassOf(const CClassTypeInfo* classInfo) const
{
    return classInfo->IsCObject();
}

END_NCBI_SCOPE
