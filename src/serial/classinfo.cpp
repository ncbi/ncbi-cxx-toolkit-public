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
* Revision 1.75  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.74  2004/03/25 15:57:08  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.73  2003/11/13 14:07:37  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.72  2003/10/01 14:40:12  vasilche
* Fixed CanGet() for members wihout 'set' flag.
*
* Revision 1.71  2003/09/16 14:48:35  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.70  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.69  2003/06/24 20:57:36  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.68  2003/05/14 14:33:02  gouriano
* added using "setflag" for implicit members
*
* Revision 1.67  2003/04/29 18:30:36  gouriano
* object data member initialization verification
*
* Revision 1.66  2003/04/10 20:13:39  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.64  2003/03/10 18:54:24  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.63  2002/11/14 20:56:21  gouriano
* moved AddMember method into the base class
*
* Revision 1.62  2002/05/22 14:03:42  grichenk
* CSerialUserOp -- added prefix UserOp_ to Assign() and Equals()
*
* Revision 1.61  2001/07/25 19:13:04  grichenk
* Check if the object is CObject-derived before dynamic cast
*
* Revision 1.60  2001/07/16 16:22:51  grichenk
* Added CSerialUserOp class to create Assign() and Equals() methods for
* user-defind classes.
* Added SerialAssign<>() and SerialEquals<>() functions.
*
* Revision 1.59  2001/05/17 15:07:04  lavr
* Typos corrected
*
* Revision 1.58  2000/10/20 15:51:37  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.57  2000/10/17 18:45:33  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.56  2000/10/13 20:22:53  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.55  2000/10/13 16:28:38  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.54  2000/10/03 17:22:41  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.53  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.52  2000/09/18 20:00:21  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.51  2000/09/01 13:16:14  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.50  2000/08/15 19:44:46  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.49  2000/07/03 20:47:22  vasilche
* Removed unused variables/functions.
*
* Revision 1.48  2000/07/03 18:42:42  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbimtx.hpp>

#include <serial/classinfo.hpp>
#include <serial/member.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>
#include <serial/stdtypes.hpp>
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE

CClassTypeInfo::CClassTypeInfo(size_t size, const char* name,
                               const void* nonCObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, nonCObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

CClassTypeInfo::CClassTypeInfo(size_t size, const char* name,
                               const CObject* cObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, cObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

CClassTypeInfo::CClassTypeInfo(size_t size, const string& name,
                               const void* nonCObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, nonCObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

CClassTypeInfo::CClassTypeInfo(size_t size, const string& name,
                               const CObject* cObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, cObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

void CClassTypeInfo::InitClassTypeInfo(void)
{
    m_ClassType = eSequential;
    m_ParentClassInfo = 0;

    UpdateFunctions();
}

CClassTypeInfo* CClassTypeInfo::SetRandomOrder(bool random)
{
    _ASSERT(!Implicit());
    m_ClassType = random? eRandom: eSequential;
    UpdateFunctions();
    return this;
}

CClassTypeInfo* CClassTypeInfo::SetImplicit(void)
{
    m_ClassType = eImplicit;
    UpdateFunctions();
    return this;
}

bool CClassTypeInfo::IsImplicitNonEmpty(void) const
{
    _ASSERT(Implicit());
    return GetImplicitMember()->NonEmpty();
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

const CClassTypeInfo* CClassTypeInfo::GetParentClassInfo(void) const
{
    return m_ParentClassInfo;
}

void CClassTypeInfo::SetParentClass(TTypeInfo parentType)
{
    if ( parentType->GetTypeFamily() != eTypeFamilyClass )
        NCBI_THROW(CSerialException,eInvalidData,
                   string("invalid parent class type: ") +
                   parentType->GetName());
    const CClassTypeInfo* parentClass =
        CTypeConverter<CClassTypeInfo>::SafeCast(parentType);
    _ASSERT(parentClass != 0);
    _ASSERT(IsCObject() == parentClass->IsCObject());
    _ASSERT(!m_ParentClassInfo);
    m_ParentClassInfo = parentClass;
    _ASSERT(GetMembers().Empty());
    AddMember(NcbiEmptyString, 0, parentType)->SetParentClass();
}

TTypeInfo CClassTypeInfo::GetRealTypeInfo(TConstObjectPtr object) const
{
    if ( !m_SubClasses.get() ) {
        // do not have subclasses -> real type is the same as our type
        return this;
    }
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
            TTypeInfo subClass = i->second.Get();
            if ( subClass->GetTypeFamily() == eTypeFamilyClass ) {
                CTypeConverter<CClassTypeInfo>::SafeCast(subClass)->RegisterSubClasses();
            }
        }
    }
}

static inline
TObjectPtr GetMember(const CMemberInfo* memberInfo, TObjectPtr object)
{
    if ( memberInfo->CanBeDelayed() )
        memberInfo->GetDelayBuffer(object).Update();
    return memberInfo->GetItemPtr(object);
}

static inline
TConstObjectPtr GetMember(const CMemberInfo* memberInfo,
                          TConstObjectPtr object)
{
    if ( memberInfo->CanBeDelayed() )
        const_cast<CDelayBuffer&>(memberInfo->GetDelayBuffer(object)).Update();
    return memberInfo->GetItemPtr(object);
}

void CClassTypeInfo::AssignMemberDefault(TObjectPtr object,
                                         const CMemberInfo* info) const
{
    // check 'set' flag
    bool haveSetFlag = info->HaveSetFlag();
    if ( haveSetFlag && info->GetSetFlagNo(object) )
        return; // member not set
    
    TObjectPtr member = GetMember(info, object);
    // assign member default
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
        info->UpdateSetFlagNo(object);
}

void CClassTypeInfo::AssignMemberDefault(TObjectPtr object,
                                         TMemberIndex index) const
{
    AssignMemberDefault(object, GetMemberInfo(index));
}


const CMemberInfo* CClassTypeInfo::GetImplicitMember(void) const
{
    _ASSERT(GetMembers().FirstIndex() == GetMembers().LastIndex());
    return GetMemberInfo(GetMembers().FirstIndex());
}

void CClassTypeInfo::UpdateFunctions(void)
{
    switch ( m_ClassType ) {
    case eSequential:
        SetReadFunction(&ReadClassSequential);
        SetWriteFunction(&WriteClassSequential);
        SetCopyFunction(&CopyClassSequential);
        SetSkipFunction(&SkipClassSequential);
        break;
    case eRandom:
        SetReadFunction(&ReadClassRandom);
        SetWriteFunction(&WriteClassRandom);
        SetCopyFunction(&CopyClassRandom);
        SetSkipFunction(&SkipClassRandom);
        break;
    case eImplicit:
        SetReadFunction(&ReadImplicitMember);
        SetWriteFunction(&WriteImplicitMember);
        SetCopyFunction(&CopyImplicitMember);
        SetSkipFunction(&SkipImplicitMember);
        break;
    }
}

void CClassTypeInfo::ReadClassSequential(CObjectIStream& in,
                                         TTypeInfo objectType,
                                         TObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.ReadClassSequential(classType, objectPtr);
}

void CClassTypeInfo::ReadClassRandom(CObjectIStream& in,
                                     TTypeInfo objectType,
                                     TObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.ReadClassRandom(classType, objectPtr);
}

void CClassTypeInfo::ReadImplicitMember(CObjectIStream& in,
                                        TTypeInfo objectType,
                                        TObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    if( memberInfo->HaveSetFlag()) {
        memberInfo->UpdateSetFlagYes(objectPtr);
    }
    in.ReadNamedType(classType,
                     memberInfo->GetTypeInfo(),
                     memberInfo->GetItemPtr(objectPtr));
}

void CClassTypeInfo::WriteClassRandom(CObjectOStream& out,
                                      TTypeInfo objectType,
                                      TConstObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    out.WriteClassRandom(classType, objectPtr);
}

void CClassTypeInfo::WriteClassSequential(CObjectOStream& out,
                                          TTypeInfo objectType,
                                          TConstObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    out.WriteClassSequential(classType, objectPtr);
}

void CClassTypeInfo::WriteImplicitMember(CObjectOStream& out,
                                         TTypeInfo objectType,
                                         TConstObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    if (memberInfo->HaveSetFlag() && memberInfo->GetSetFlagNo(objectPtr)) {
        if (memberInfo->Optional()) {
            return;
        }
        if (memberInfo->NonEmpty() ||
            memberInfo->GetTypeInfo()->GetTypeFamily() != eTypeFamilyContainer) {
            ESerialVerifyData verify = out.GetVerifyData();
            if (verify == eSerialVerifyData_Yes) {
                out.ThrowError(CObjectOStream::fUnassigned,
                    string("Unassigned member: ")+classType->GetName());
            } else if (verify == eSerialVerifyData_No) {
                return;
            }
        } 
    }
    out.WriteNamedType(classType,
                       memberInfo->GetTypeInfo(),
                       memberInfo->GetItemPtr(objectPtr));
}

void CClassTypeInfo::CopyClassRandom(CObjectStreamCopier& copier,
                                     TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    copier.CopyClassRandom(classType);
}

void CClassTypeInfo::CopyClassSequential(CObjectStreamCopier& copier,
                                         TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    copier.CopyClassSequential(classType);
}

void CClassTypeInfo::CopyImplicitMember(CObjectStreamCopier& copier,
                                        TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    copier.CopyNamedType(classType, memberInfo->GetTypeInfo());
}

void CClassTypeInfo::SkipClassRandom(CObjectIStream& in,
                                     TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.SkipClassRandom(classType);
}

void CClassTypeInfo::SkipClassSequential(CObjectIStream& in,
                                         TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.SkipClassSequential(classType);
}

void CClassTypeInfo::SkipImplicitMember(CObjectIStream& in,
                                        TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    in.SkipNamedType(classType, memberInfo->GetTypeInfo());
}

bool CClassTypeInfo::IsDefault(TConstObjectPtr /*object*/) const
{
    return false;
}

void CClassTypeInfo::SetDefault(TObjectPtr dst) const
{
    for ( TMemberIndex i = GetMembers().FirstIndex(),
              last = GetMembers().LastIndex();
          i <= last; ++i ) {
        AssignMemberDefault(dst, i);
    }
}

bool CClassTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                            ESerialRecursionMode how) const
{
    for ( TMemberIndex i = GetMembers().FirstIndex(),
              last = GetMembers().LastIndex();
          i <= last; ++i ) {
        const CMemberInfo* info = GetMemberInfo(i);
        if ( !info->GetTypeInfo()->Equals(GetMember(info, object1),
                                          GetMember(info, object2), how) )
            return false;
        if ( info->HaveSetFlag() ) {
            if ( !info->CompareSetFlags(object1,object2) )
                return false;
        }
    }

    // User defined comparison
    if ( IsCObject() ) {
        const CSerialUserOp* op1 =
            dynamic_cast<const CSerialUserOp*>
            (static_cast<const CObject*>(object1));
        const CSerialUserOp* op2 =
            dynamic_cast<const CSerialUserOp*>
            (static_cast<const CObject*>(object2));
        if ( op1  &&  op2 ) {
            return op1->UserOp_Equals(*op2);
        }
    }
    return true;
}

void CClassTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                            ESerialRecursionMode how) const
{
    for ( TMemberIndex i = GetMembers().FirstIndex(),
              last = GetMembers().LastIndex();
          i <= last; ++i ) {
        const CMemberInfo* info = GetMemberInfo(i);
        info->GetTypeInfo()->Assign(GetMember(info, dst),
                                    GetMember(info, src), how);
        if ( info->HaveSetFlag() ) {
            info->UpdateSetFlag(dst,info->GetSetFlag(src));
        }
    }

    // User defined assignment
    if ( IsCObject() ) {
        const CSerialUserOp* opsrc =
            dynamic_cast<const CSerialUserOp*>
            (static_cast<const CObject*>(src));
        CSerialUserOp* opdst =
            dynamic_cast<CSerialUserOp*>
            (static_cast<CObject*>(dst));
        if ( opdst  &&  opsrc ) {
            opdst->UserOp_Assign(*opsrc);
        }
    }
}

bool CClassTypeInfo::IsType(TTypeInfo typeInfo) const
{
    return typeInfo == this || typeInfo->IsParentClassOf(this);
}

bool CClassTypeInfo::IsParentClassOf(const CClassTypeInfo* typeInfo) const
{
    do {
        typeInfo = typeInfo->m_ParentClassInfo;
        if ( typeInfo == this )
            return true;
    } while ( typeInfo );
    return false;
}

bool CClassTypeInfo::CalcMayContainType(TTypeInfo typeInfo) const
{
    const CClassTypeInfoBase* parentClass = m_ParentClassInfo;
    return parentClass && parentClass->MayContainType(typeInfo) ||
        CParent::CalcMayContainType(typeInfo);
}

END_NCBI_SCOPE
