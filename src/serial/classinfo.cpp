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
#include <serial/iterator.hpp>
#include <set>

BEGIN_NCBI_SCOPE

static
string ClassName(const type_info& id)
{
    const char* s = id.name();
    if ( memcmp(s, "class ", 6) == 0 ) {
        // MSVC
        return s + 6;
    }
    else if ( isdigit(*s ) ) {
        // GCC
        while ( isdigit(*s) )
            ++s;
        return s;
    }
    else {
        return s;
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

static
bool IsMemberDefault(TConstObjectPtr object,
                     const CMembersInfo& members, size_t index)
{
    const CMemberInfo* info = members.GetMemberInfo(index);
    if ( info->HaveSetFlag() ) {
        return !info->GetSetFlag(object);
    }
    else if ( info->Optional() ) {
        TConstObjectPtr def = info->GetDefault();
        TTypeInfo typeInfo = info->GetTypeInfo();
        TConstObjectPtr member = info->GetMember(object);
        if ( !def ) {
            return typeInfo->IsDefault(member);
        }
        else {
            return typeInfo->Equals(member, def);
        }
    }
    return false;
}

static
void AssignMemberDefault(CObjectIStream& in, TObjectPtr object,
                         const CMembersInfo& members, size_t index)
{
    const CMemberInfo* info = members.GetMemberInfo(index);
    if ( !info->Optional() ) {
        in.ThrowError(in.eFormatError,
                      "member "+members.GetMemberId(index).ToString()+
                      " expected");
    }
    bool haveSetFlag = info->HaveSetFlag();
    if ( haveSetFlag && !info->GetSetFlag(object) )
        return; // member not set

    TObjectPtr member = info->GetMember(object);
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

static
void AssignMemberDefault(TObjectPtr object, const CMemberInfo* info)
{
    // check 'set' flag
    bool haveSetFlag = info->HaveSetFlag();
    if ( haveSetFlag && !info->GetSetFlag(object) )
        return; // member not set

    TObjectPtr member = info->GetMember(object);
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

static inline
void SkipMember(CObjectIStream& in,
                const CMembersInfo& members, size_t index)
{
    const CMemberInfo* info = members.GetMemberInfo(index);
    // skip member data
    info->GetTypeInfo()->SkipData(in);
}

static
void ReadMember(CObjectIStream& in, TObjectPtr object,
                const CMembersInfo& members, size_t index)
{
    const CMemberInfo* info = members.GetMemberInfo(index);
    TObjectPtr member = info->GetMember(object);
    // read member data
    info->GetTypeInfo()->ReadData(in, member);
    // update 'set' flag
    if ( info->HaveSetFlag() )
        info->GetSetFlag(object) = true;
}

CClassInfoTmpl::CClassInfoTmpl(const type_info& id, size_t size)
    : CParent(ClassName(id)), m_Id(id), m_Size(size),
      m_RandomOrder(false), m_Implicit(false), m_ParentClassInfo(0)
{
    Register();
}

CClassInfoTmpl::CClassInfoTmpl(const string& name, const type_info& id,
                               size_t size)
    : CParent(name), m_Id(id), m_Size(size),
      m_RandomOrder(false), m_Implicit(false), m_ParentClassInfo(0)
{
    Register();
}

CClassInfoTmpl::CClassInfoTmpl(const char* name, const type_info& id,
                               size_t size)
    : CParent(name), m_Id(id), m_Size(size),
      m_RandomOrder(false), m_Implicit(false), m_ParentClassInfo(0)
{
    Register();
}

CClassInfoTmpl::~CClassInfoTmpl(void)
{
    Deregister();
}

CClassInfoTmpl::TClasses* CClassInfoTmpl::sm_Classes = 0;
CClassInfoTmpl::TClassesById* CClassInfoTmpl::sm_ClassesById = 0;
CClassInfoTmpl::TClassesByName* CClassInfoTmpl::sm_ClassesByName = 0;

inline
CClassInfoTmpl::TClasses& CClassInfoTmpl::Classes(void)
{
    TClasses* classes = sm_Classes;
    if ( !classes ) {
        classes = sm_Classes = new TClasses;
    }
    return *classes;
}

inline
CClassInfoTmpl::TClassesById& CClassInfoTmpl::ClassesById(void)
{
    TClassesById* classes = sm_ClassesById;
    if ( !classes ) {
        classes = sm_ClassesById = new TClassesById;
        const TClasses& cc = Classes();
        for ( TClasses::const_iterator i = cc.begin(); i != cc.end(); ++i ) {
            const CClassInfoTmpl* info = *i;
            if ( info->GetId() != typeid(void) ) {
                _TRACE("class by id: " << info->GetId().name()
                       << " : " << info->GetName());
                if ( !classes->insert(
                         TClassesById::value_type(&info->GetId(),
                                                  info)).second ) {
                    THROW1_TRACE(runtime_error,
                                 "duplicated class ids " +
                                 string(info->GetId().name()));
                }
            }
        }
    }
    return *classes;
}

inline
CClassInfoTmpl::TClassesByName& CClassInfoTmpl::ClassesByName(void)
{
    TClassesByName* classes = sm_ClassesByName;
    if ( !classes ) {
        classes = sm_ClassesByName = new TClassesByName;
        const TClasses& cc = Classes();
        for ( TClasses::const_iterator i = cc.begin(); i != cc.end(); ++i ) {
            const CClassInfoTmpl* info = *i;
            if ( !info->GetName().empty() ) {
                _TRACE("class by name: " << " : " <<
                       info->GetName() << info->GetId().name());
                if ( !classes->insert(
                         TClassesByName::value_type(info->GetName(),
                                                    info)).second ) {
                    THROW1_TRACE(runtime_error,
                                 "duplicated class names " + info->GetName());
                }
            }
        }
    }
    return *classes;
}

void CClassInfoTmpl::Register(void)
{
    delete sm_ClassesById;
    sm_ClassesById = 0;
    delete sm_ClassesByName;
    sm_ClassesByName = 0;
    Classes().push_back(this);
}

void CClassInfoTmpl::Deregister(void) const
{
}

void CClassInfoTmpl::RegisterSubClasses(void) const
{
    const TSubClasses* subclasses = m_SubClasses.get();
    if ( subclasses ) {
        for ( TSubClasses::const_iterator i = subclasses->begin();
              i != subclasses->end();
              ++i ) {
            const CClassInfoTmpl* classInfo = 
                dynamic_cast<const CClassInfoTmpl*>(i->second.Get());
            if ( classInfo )
                classInfo->RegisterSubClasses();
        }
    }
}

TTypeInfo CClassInfoTmpl::GetClassInfoById(const type_info& id)
{
    TClassesById& types = ClassesById();
    TClassesById::iterator i = types.find(&id);
    if ( i == types.end() ) {
        THROW1_TRACE(runtime_error, "class not found: " + string(id.name()));
    }
    return i->second;
}

TTypeInfo CClassInfoTmpl::GetClassInfoByName(const string& name)
{
    TClassesByName& classes = ClassesByName();
    TClassesByName::iterator i = classes.find(name);
    if ( i == classes.end() ) {
        THROW1_TRACE(runtime_error, "class not found: " + name);
    }
    return i->second;
}

TTypeInfo CClassInfoTmpl::GetRealTypeInfo(TConstObjectPtr object) const
{
    const type_info* ti = GetCPlusPlusTypeInfo(object);
    if ( ti == 0 || ti == &GetId() )
        return this;
    RegisterSubClasses();
    return GetClassInfoById(*ti);
}

size_t CClassInfoTmpl::GetSize(void) const
{
    return m_Size;
}

void CClassInfoTmpl::UpdateClassInfo(const CObject* object)
{
    SetParentClass(CObject::GetTypeInfo());
}

void CClassInfoTmpl::SetParentClass(TTypeInfo parentType)
{
    const CClassInfoTmpl* parentClass =
        dynamic_cast<const CClassInfoTmpl*>(parentType);
    _ASSERT(parentClass != 0);
    _ASSERT(!m_ParentClassInfo || m_ParentClassInfo == CObject::GetTypeInfo());
    m_ParentClassInfo = parentClass;
}

void CClassInfoTmpl::AddSubClass(const CMemberId& id,
                                 const CTypeRef& type)
{
    TSubClasses* subclasses = m_SubClasses.get();
    if ( !subclasses )
        m_SubClasses.reset(subclasses = new TSubClasses);
    subclasses->push_back(make_pair(id, type));
}

void CClassInfoTmpl::AddSubClassNull(const CMemberId& id)
{
    AddSubClass(id, CTypeRef(TTypeInfo(0)));
}

void CClassInfoTmpl::AddSubClass(const char* id, TTypeInfoGetter getter)
{
    AddSubClass(CMemberId(id), getter);
}

void CClassInfoTmpl::AddSubClassNull(const char* id)
{
    AddSubClassNull(CMemberId(id));
}

inline
const CMemberInfo* GetImplicitMember(const CMembersInfo& members)
{
    _ASSERT(members.GetSize() == 1);
    return members.GetMemberInfo(0);
}

void CClassInfoTmpl::WriteData(CObjectOStream& out,
                               TConstObjectPtr object) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info = GetImplicitMember(m_Members);
        info->GetTypeInfo()->WriteData(out, info->GetMember(object));
    }
    else {
        CObjectOStream::Block block(out, RandomOrder());
        WriteMembers(out, block, object);
    }
}

void CClassInfoTmpl::SkipData(CObjectIStream& in) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info = GetImplicitMember(m_Members);
        info->GetTypeInfo()->SkipData(in);
    }
    else {
        CObjectIStream::Block block(in, RandomOrder());
        SkipMembers(in, block);
    }
}

void CClassInfoTmpl::ReadData(CObjectIStream& in, TObjectPtr object) const
{
    if ( Implicit() ) {
        // special case: class contains only one implicit member
        // we'll behave as this one member
        const CMemberInfo* info = GetImplicitMember(m_Members);
        info->GetTypeInfo()->ReadData(in, info->GetMember(object));
    }
    else {
        CObjectIStream::Block block(in, RandomOrder());
        ReadMembers(in, block, object);
    }
}

void CClassInfoTmpl::WriteMembers(CObjectOStream& out,
                                  CObjectOStream::Block& block,
                                  TConstObjectPtr object) const
{
    if ( m_ParentClassInfo )
        m_ParentClassInfo->WriteMembers(out, block, object);
    for ( TMemberIndex i = 0, size = m_Members.GetSize(); i < size; ++i ) {
        if ( !IsMemberDefault(object, m_Members, i) ) {
            // write member
            block.Next();
            CObjectOStream::Member m(out, m_Members, i);
            const CMemberInfo* info = m_Members.GetMemberInfo(i);
            info->GetTypeInfo()->WriteData(out, info->GetMember(object));
        }
    }
}

void CClassInfoTmpl::SkipMembers(CObjectIStream& in,
                                 CObjectIStream::Block& block) const
{
    if ( m_ParentClassInfo && m_ParentClassInfo != CObject::GetTypeInfo() )
        m_ParentClassInfo->SkipMembers(in, block);
    if ( RandomOrder() ) {
        vector<bool> read(m_Members.GetSize());
        while ( block.Next() ) {
            // get next member
            CObjectIStream::Member m(in, m_Members);
            size_t index = m.GetIndex();
            if ( read[index] ) {
                in.ThrowError(in.eFormatError,
                              "duplicated member: "+m.ToString());
            }
            read[index] = true;
            SkipMember(in, m_Members, index);
            m.End();
        }
        // skip all absent members
        for ( size_t i = 0, end = m_Members.GetSize(); i < end; ++i ) {
            if ( !read[i] ) {
                // check if this member have defaults
                CheckMemberOptional(in, m_Members, i);
            }
        }
    }
    else {
        // sequential order
        CObjectIStream::LastMember lastMember(m_Members);
        while ( block.Next() ) {
            // get next member
            CObjectIStream::Member m(in, lastMember);
            // find desired member
            size_t index = m.GetIndex();
            for ( size_t i = lastMember.GetIndex() + 1; i < index; ++i ) {
                // init missing member
                CheckMemberOptional(in, m_Members, i);
            }
            SkipMember(in, m_Members, index);
            m.End();
        }
        // init all absent members
        for ( size_t i = lastMember.GetIndex() + 1, end = m_Members.GetSize();
              i < end; ++i ) {
            CheckMemberOptional(in, m_Members, i);
        }
    }
}

void CClassInfoTmpl::ReadMembers(CObjectIStream& in,
                                 CObjectIStream::Block& block,
                                 TObjectPtr object) const
{
    if ( m_ParentClassInfo && m_ParentClassInfo != CObject::GetTypeInfo() )
        m_ParentClassInfo->ReadMembers(in, block, object);
    if ( RandomOrder() ) {
        vector<bool> read(m_Members.GetSize());
        while ( block.Next() ) {
            // get next member
            CObjectIStream::Member m(in, m_Members);
            size_t index = m.GetIndex();
            if ( read[index] ) {
                in.ThrowError(in.eFormatError,
                              "duplicated member: "+m.ToString());
            }
            read[index] = true;
            ReadMember(in, object, m_Members, index);
            m.End();
        }
        // init all absent members
        for ( size_t i = 0, end = m_Members.GetSize(); i < end; ++i ) {
            if ( !read[i] ) {
                // check if this member have defaults
                AssignMemberDefault(in, object, m_Members, i);
            }
        }
    }
    else {
        // sequential order
        size_t currentMember = 0;
        CObjectIStream::LastMember lastMember(m_Members);
        while ( block.Next() ) {
            // get next member
            CObjectIStream::Member m(in, lastMember);
            // find desired member
            size_t fileMember = m.GetIndex();
            while ( currentMember < fileMember ) {
                // init missing member
                AssignMemberDefault(in, object, m_Members, currentMember++);
            }
            ReadMember(in, object, m_Members, currentMember++);
            m.End();
        }
        // init all absent members
        size_t memberCount = m_Members.GetSize();
        while ( currentMember < memberCount ) {
            AssignMemberDefault(in, object, m_Members, currentMember++);
        }
    }
}

const type_info*
CCObjectClassInfo::GetCPlusPlusTypeInfo(TConstObjectPtr object) const
{
    return &typeid(*static_cast<const CObject*>(object));
}

TTypeInfo CObject::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CCObjectClassInfo;
    return typeInfo;
}

TTypeInfo CClassInfoTmpl::GetParentTypeInfo(void) const
{
    return m_ParentClassInfo;
}

CTypeInfo::TMemberIndex CClassInfoTmpl::FindMember(const string& name) const
{
    return m_Members.FindMember(name);
}

const CMemberId* CClassInfoTmpl::GetMemberId(TMemberIndex index) const
{
    return &m_Members.GetMemberId(index);
}

const CMemberInfo* CClassInfoTmpl::GetMemberInfo(TMemberIndex index) const
{
    return m_Members.GetMemberInfo(index);
}

bool CClassInfoTmpl::IsDefault(TConstObjectPtr /*object*/) const
{
    return false;
#if 0
    for ( TMemberIndex i = 0, size = m_Members.GetSize(); i < size; ++i ) {
        if ( !IsMemberDefault(object, m_Members, i) )
            return false;
    }
    return true;
#endif
}

void CClassInfoTmpl::SetDefault(TObjectPtr dst) const
{
    for ( TMemberIndex i = 0, size = m_Members.GetSize(); i < size; ++i ) {
        AssignMemberDefault(dst, m_Members, i);
    }
}

bool CClassInfoTmpl::Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
{
    for ( TMemberIndex i = 0, size = m_Members.GetSize(); i < size; ++i ) {
        const CMemberInfo& member = *m_Members.GetMemberInfo(i);
        if ( !member.GetTypeInfo()->Equals(member.GetMember(object1),
                                           member.GetMember(object2)) )
            return false;
        if ( member.HaveSetFlag() ) {
            if ( member.GetSetFlag(object1) != member.GetSetFlag(object2) )
                return false;
        }
    }
    return true;
}

void CClassInfoTmpl::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    for ( TMemberIndex i = 0, size = m_Members.GetSize(); i < size; ++i ) {
        const CMemberInfo& member = *m_Members.GetMemberInfo(i);
        member.GetTypeInfo()->Assign(member.GetMember(dst),
                                     member.GetMember(src));
        if ( member.HaveSetFlag() ) {
            member.GetSetFlag(dst) = member.GetSetFlag(src);
        }
    }
}

const type_info* CClassInfoTmpl::GetCPlusPlusTypeInfo(TConstObjectPtr ) const
{
    return 0;
}

bool CClassInfoTmpl::IsType(TTypeInfo typeInfo) const
{
    if ( typeInfo == this )
        return true;
    TTypeInfo parentType = m_ParentClassInfo;
    if ( parentType )
        return parentType->IsType(typeInfo);
    return false;
}

bool CClassInfoTmpl::MayContainType(TTypeInfo typeInfo) const
{
    TContainedTypes* cache = m_ContainedTypes.get();
    if ( cache ) {
        TContainedTypes::const_iterator found = cache->find(typeInfo);
        if ( found != cache->end() )
            return found->second;
        (*cache)[typeInfo] = false;
    }
    else {
        m_ContainedTypes.reset(cache = new TContainedTypes);
    }
    bool contains = GetMembers().MayContainType(typeInfo);
    (*cache)[typeInfo] = contains;
    return contains;
}

bool CClassInfoTmpl::HaveChildren(TConstObjectPtr object) const
{
    return GetMembers().HaveChildren(object);
}

void CClassInfoTmpl::BeginTypes(CChildrenTypesIterator& cc) const
{
    GetMembers().BeginTypes(cc);
}

void CClassInfoTmpl::Begin(CConstChildrenIterator& cc) const
{
    GetMembers().Begin(cc);
}

void CClassInfoTmpl::Begin(CChildrenIterator& cc) const
{
    GetMembers().Begin(cc);
}

bool CClassInfoTmpl::ValidTypes(const CChildrenTypesIterator& cc) const
{
    return GetMembers().ValidTypes(cc);
}

bool CClassInfoTmpl::Valid(const CConstChildrenIterator& cc) const
{
    return GetMembers().Valid(cc);
}

bool CClassInfoTmpl::Valid(const CChildrenIterator& cc) const
{
    return GetMembers().Valid(cc);
}

TTypeInfo CClassInfoTmpl::GetChildType(const CChildrenTypesIterator& cc) const
{
    return GetMembers().GetChildType(cc);
}

void CClassInfoTmpl::GetChild(const CConstChildrenIterator& cc,
                                CConstObjectInfo& child) const
{
    GetMembers().GetChild(cc, child);
}

void CClassInfoTmpl::GetChild(const CChildrenIterator& cc,
                         CObjectInfo& child) const
{
    GetMembers().GetChild(cc, child);
}

void CClassInfoTmpl::NextType(CChildrenTypesIterator& cc) const
{
    GetMembers().NextType(cc);
}

void CClassInfoTmpl::Next(CConstChildrenIterator& cc) const
{
    GetMembers().Next(cc);
}

void CClassInfoTmpl::Next(CChildrenIterator& cc) const
{
    GetMembers().Next(cc);
}

void CClassInfoTmpl::Erase(CChildrenIterator& cc) const
{
    const CMemberInfo* info = GetMembers().GetMemberInfo(cc);
    if ( info->GetDefault() )
        THROW1_TRACE(runtime_error, "cannot erase member with default value");

    AssignMemberDefault(cc.GetParentPtr(), info);
}

TObjectPtr CStructInfoTmpl::Create(void) const
{
    TObjectPtr object = calloc(GetSize(), 1);
    if ( object == 0 )
        THROW_TRACE(bad_alloc, ());
    _TRACE("Create: " << GetName() << ": " << NStr::PtrToString(object));
    return object;
}

CGeneratedClassInfo::CGeneratedClassInfo(const char* name,
                                         const type_info& typeId,
                                         size_t size,
                                         TCreateFunction cF,
                                         TGetTypeIdFunction gTIF)
    : CParent(name, typeId, size),
      m_CreateFunction(cF), m_GetTypeIdFunction(gTIF),
      m_PostReadFunction(0), m_PreWriteFunction(0)
{
}

void CGeneratedClassInfo::SetPostRead(TPostReadFunction func)
{
    _ASSERT(m_PostReadFunction == 0);
    _ASSERT(func != 0);
    m_PostReadFunction = func;
}

void CGeneratedClassInfo::SetPreWrite(TPreWriteFunction func)
{
    _ASSERT(m_PreWriteFunction == 0);
    _ASSERT(func != 0);
    m_PreWriteFunction = func;
}

void DoSetPostRead(CGeneratedClassInfo* info,
                   void (*func)(TObjectPtr object))
{
    info->SetPostRead(func);
}

void DoSetPreWrite(CGeneratedClassInfo* info,
                   void (*func)(TConstObjectPtr object))
{
    info->SetPreWrite(func);
}

TObjectPtr CGeneratedClassInfo::Create(void) const
{
    if ( m_CreateFunction )
        return m_CreateFunction();
    return CParent::Create();
}

const type_info* CGeneratedClassInfo::GetCPlusPlusTypeInfo(TConstObjectPtr object) const
{
    return m_GetTypeIdFunction(object);
}

void CGeneratedClassInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    if ( m_PreWriteFunction )
        m_PreWriteFunction(object);
    CParent::WriteData(out, object);
}

void CGeneratedClassInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    CParent::ReadData(in, object);
    if ( m_PostReadFunction )
        m_PostReadFunction(object);
}

END_NCBI_SCOPE
