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
*   Serialization classes.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.28  2005/02/24 14:39:04  gouriano
* Added PreRead/PostWrite hooks
*
* Revision 1.27  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.26  2003/08/14 20:03:58  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.25  2003/04/10 20:13:39  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.23  2003/03/06 21:48:41  grichenk
* Removed type-info cleanup code
*
* Revision 1.22  2002/11/14 21:01:11  gouriano
* modified AddMember to use CClassTypeInfoBase
*
* Revision 1.21  2002/11/05 17:45:10  grichenk
* Reset object before reading
*
* Revision 1.20  2002/10/25 15:05:44  vasilche
* Moved more code to libxcser library.
*
* Revision 1.19  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.18  2002/09/05 21:22:51  vasilche
* Added mutex for module names set
*
* Revision 1.17  2001/08/24 13:48:02  grichenk
* Prevented some memory leaks
*
* Revision 1.16  2000/11/07 17:25:41  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.15  2000/10/13 20:22:56  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.14  2000/09/26 17:38:23  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.13  2000/09/18 20:00:25  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.12  2000/09/01 13:16:20  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.11  2000/08/15 19:44:51  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/11 20:36:19  vasilche
* File included in all generated headers made lighter.
* Nonnecessary code moved to serialimpl.hpp.
*
* Revision 1.9  2000/07/03 18:42:47  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.8  2000/06/16 16:31:22  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.7  2000/02/01 21:47:23  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.6  1999/12/28 21:04:25  vasilche
* Removed three more implicit virtual destructors.
*
* Revision 1.5  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.4  1999/11/22 21:04:41  vasilche
* Cleaned main interface headers. Now generated files should include serial/serialimpl.hpp and user code should include serial/serial.hpp which became might lighter.
*
* Revision 1.3  1999/06/04 20:51:49  vasilche
* First compilable version of serialization.
*
* Revision 1.2  1999/05/19 19:56:57  vasilche
* Commit just in case.
*
* Revision 1.1  1999/03/25 19:12:04  vasilche
* Beginning of serialization library.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <serial/serial.hpp>
#include <serial/serialimpl.hpp>
#include <serial/serialbase.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/memberlist.hpp>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_NCBI_SCOPE

TTypeInfo CPointerTypeInfoGetTypeInfo(TTypeInfo type)
{
    return CPointerTypeInfo::GetTypeInfo(type);
}

void Write(CObjectOStream& out, TConstObjectPtr object, const CTypeRef& type)
{
    out.Write(object, type.Get());
}

void Read(CObjectIStream& in, TObjectPtr object, const CTypeRef& type)
{
    //type.Get()->SetDefault(object);
    in.Read(object, type.Get());
}

DEFINE_STATIC_FAST_MUTEX(s_ModuleNameMutex);

static const string& GetModuleName(const char* moduleName)
{
    CFastMutexGuard GUARD(s_ModuleNameMutex);
    static CSafeStaticPtr< set<string> > s_ModuleNames;
    return *s_ModuleNames.Get().insert(moduleName).first;
}

void SetModuleName(CTypeInfo* info, const char* moduleName)
{
    info->SetModuleName(GetModuleName(moduleName));
}

void SetModuleName(CEnumeratedTypeValues* info, const char* moduleName)
{
    info->SetModuleName(GetModuleName(moduleName));
}

// add member functions
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       const CTypeRef& r)
{
    return info->AddMember(name, member, r);
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfo t)
{
    return AddMember(info, name, member, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, CTypeRef(f));
}

// two arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f1, CTypeRef(f));
}

// three arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, f2, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f2, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f2, f1, CTypeRef(f));
}

// four arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, f3, f2, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f3, f2, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f3, f2, f1, CTypeRef(f));
}

// five arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, f4, f3, f2, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f4, f3, f2, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f4, f3, f2, f1, CTypeRef(f));
}

// add variant functions
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       const CTypeRef& r)
{
    return info->AddVariant(name, member, r);
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, CTypeRef(f));
}

// two arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f1, CTypeRef(f));
}

// three arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, f2, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f2, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f2, f1, CTypeRef(f));
}

// four arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, f3, f2, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f3, f2, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f3, f2, f1, CTypeRef(f));
}

// five arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, f4, f3, f2, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f4, f3, f2, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f4, f3, f2, f1, CTypeRef(f));
}



CChoiceTypeInfo*
CClassInfoHelperBase::CreateChoiceInfo(const char* name, size_t size,
                                       const void* nonCObject,
                                       TCreateFunction createFunc,
                                       const type_info& ti,
                                       TWhichFunction whichFunc,
                                       TSelectFunction selectFunc,
                                       TResetFunction resetFunc)
{
    return new CChoiceTypeInfo(size, name, nonCObject, createFunc,
                               ti, whichFunc, selectFunc, resetFunc);
}

CChoiceTypeInfo*
CClassInfoHelperBase::CreateChoiceInfo(const char* name, size_t size,
                                       const CObject* cObject,
                                       TCreateFunction createFunc,
                                       const type_info& ti,
                                       TWhichFunction whichFunc,
                                       TSelectFunction selectFunc,
                                       TResetFunction resetFunc)
{
    return new CChoiceTypeInfo(size, name, cObject, createFunc,
                               ti, whichFunc, selectFunc, resetFunc);
}

CClassTypeInfo*
CClassInfoHelperBase::CreateClassInfo(const char* name, size_t size,
                                      const void* nonCObject,
                                      TCreateFunction createFunc,
                                      const type_info& id,
                                      TGetTypeIdFunction idFunc)
{
    return new CClassTypeInfo(size, name, nonCObject, createFunc, id, idFunc);
}

CClassTypeInfo*
CClassInfoHelperBase::CreateClassInfo(const char* name, size_t size,
                                      const CObject* cObject,
                                      TCreateFunction createFunc,
                                      const type_info& id,
                                      TGetTypeIdFunction idFunc)
{
    return new CClassTypeInfo(size, name, cObject, createFunc, id, idFunc);
}

void SetPreWrite(CClassTypeInfo* info, TPreWriteFunction func)
{
    info->SetPreWriteFunction(func);
}

void SetPostWrite(CClassTypeInfo* info, TPostWriteFunction func)
{
    info->SetPostWriteFunction(func);
}

void SetPreRead(CClassTypeInfo* info, TPreReadFunction func)
{
    info->SetPreReadFunction(func);
}

void SetPostRead(CClassTypeInfo* info, TPostReadFunction func)
{
    info->SetPostReadFunction(func);
}

void SetPreWrite(CChoiceTypeInfo* info, TPreWriteFunction func)
{
    info->SetPreWriteFunction(func);
}

void SetPostWrite(CChoiceTypeInfo* info, TPostWriteFunction func)
{
    info->SetPostWriteFunction(func);
}

void SetPreRead(CChoiceTypeInfo* info, TPreReadFunction func)
{
    info->SetPreReadFunction(func);
}

void SetPostRead(CChoiceTypeInfo* info, TPostReadFunction func)
{
    info->SetPostReadFunction(func);
}


// Functions preventing memory leaks due to undestroyed type info objects

void RegisterEnumTypeValuesObject(CEnumeratedTypeValues* /*object*/)
{
/*
    typedef AutoPtr<CEnumeratedTypeValues> TEnumTypePtr;
    typedef list<TEnumTypePtr>             TEnumTypeList;

    static CSafeStaticPtr<TEnumTypeList> s_EnumTypeList;

    TEnumTypePtr ap(object);
    s_EnumTypeList.Get().push_back(ap);
*/
}

void RegisterTypeInfoObject(CTypeInfo* /*object*/)
{
/*
    typedef AutoPtr<CTypeInfo> TTypeInfoPtr;
    typedef list<TTypeInfoPtr> TTypeInfoList;

    static CSafeStaticPtr<TTypeInfoList> s_TypeInfoList;

    TTypeInfoPtr ap(object);
    s_TypeInfoList.Get().push_back(ap);
*/
}

END_NCBI_SCOPE
