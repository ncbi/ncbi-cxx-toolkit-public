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

#include <corelib/ncbistd.hpp>
#include <serial/serial.hpp>
#include <serial/serialimpl.hpp>
#include <serial/serialbase.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/memberlist.hpp>
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

TTypeInfo GetStdTypeInfo_char_ptr(void)
{
    return CStdTypeInfo<char*>::GetTypeInfo();
}

TTypeInfo GetStdTypeInfo_const_char_ptr(void)
{
    return CStdTypeInfo<const char*>::GetTypeInfo();
}

class CGet2TypeInfoSource : public CTypeInfoSource
{
public:
    CGet2TypeInfoSource(TTypeInfoGetter2 getter,
                        const CTypeRef& arg1, const CTypeRef& arg2);
    ~CGet2TypeInfoSource(void);

    virtual TTypeInfo GetTypeInfo(void);

private:
    TTypeInfoGetter2 m_Getter;
    CTypeRef m_Argument1, m_Argument2;
};

CGet2TypeInfoSource::CGet2TypeInfoSource(TTypeInfoGetter2 getter,
                                         const CTypeRef& arg1,
                                         const CTypeRef& arg2)
    : m_Getter(getter), m_Argument1(arg1), m_Argument2(arg2)
{
}

CGet2TypeInfoSource::~CGet2TypeInfoSource(void)
{
}

TTypeInfo CGet2TypeInfoSource::GetTypeInfo(void)
{
    return m_Getter(m_Argument1.Get(), m_Argument2.Get());
}
                   

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
    in.Read(object, type.Get());
}

// add member functions
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfo typeInfo)
{
    return info.AddMember(name, member, typeInfo);
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, CMemberInfo* memberInfo)
{
    return info.AddMember(name, memberInfo);
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          const CTypeRef& typeRef)
{
    return info.AddMember(name, member, typeRef);
}

// one argument:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter f)
{
    return info.AddMember(name, member, CTypeRef(f));
}

// two arguments:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfo t)
{
    return info.AddMember(name, member, CTypeRef(f, t));
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter f1)
{
    return info.AddMember(name, member, CTypeRef(f, f1));
}

// three arguments:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter1 f1, TTypeInfo t1)
{
    return info.AddMember(name, member, CTypeRef(f, CTypeRef(f1, t1)));
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter1 f1, TTypeInfoGetter f11)
{
    return info.AddMember(name, member, CTypeRef(f, CTypeRef(f1, f11)));
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfo t1, TTypeInfo t2)
{
    return info.AddMember(name, member, CTypeRef(new CGet2TypeInfoSource(f, t1, t2)));
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfoGetter f1, TTypeInfo t2)
{
    return info.AddMember(name, member, CTypeRef(new CGet2TypeInfoSource(f, f1, t2)));
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfo t1, TTypeInfoGetter f2)
{
    return info.AddMember(name, member, CTypeRef(new CGet2TypeInfoSource(f, t1, f2)));
}

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfoGetter f1, TTypeInfoGetter f2)
{
    return info.AddMember(name, member, CTypeRef(new CGet2TypeInfoSource(f, f1, f2)));
}

CChoiceTypeInfo* CClassInfoHelperBase::CreateChoiceInfo(const char* name, size_t size,
                                                        const type_info& ti,
                                                        TCreateFunction createFunc,
                                                        TWhichFunction whichFunc,
                                                        TSelectFunction selectFunc,
                                                        TResetFunction resetFunc)
{
    return new CChoiceTypeInfo(name, size, ti, createFunc,
                               whichFunc, selectFunc, resetFunc);
}

CClassTypeInfo* CClassInfoHelperBase::CreateClassInfo(const char* name, size_t size,
                                                      const type_info& id)
{
    return new CClassTypeInfo(name, id, size);
}

CClassTypeInfo* CClassInfoHelperBase::CreateClassInfo(const char* name, size_t size,
                                                      const type_info& id,
                                                      TGetTypeIdFunction func)
{
    CClassTypeInfo* info = new CClassTypeInfo(name, id, size);
    info->SetGetTypeIdFunction(func);
    return info;
}

#if HAVE_NCBI_C
static TObjectPtr CreateAsnStruct(TTypeInfo info)
{
    TObjectPtr object = calloc(info->GetSize(), 1);
    if ( !object )
        THROW_TRACE(bad_alloc, ());
    return object;
}

CClassTypeInfo* CClassInfoHelperBase::CreateAsnStructInfo(const char* name,
                                                          size_t size,
                                                          const type_info& id)
{
    CClassTypeInfo* info = CreateClassInfo(name, size, id);
    info->SetCreateFunction(&CreateAsnStruct);
    return info;
}

static int WhichAsn(TConstObjectPtr object)
{
    const valnode* node = static_cast<const valnode*>(object);
    return node->choice - 1;
}

static void SelectAsn(TObjectPtr object, int index)
{
    valnode* node = static_cast<valnode*>(object);
    node->choice = index + 1;
}

CChoiceTypeInfo* CClassInfoHelperBase::CreateAsnChoiceInfo(const char* name)
{
    return CreateChoiceInfo(name, sizeof(valnode), typeid(valnode), &CreateAsnStruct,
                            &WhichAsn, &SelectAsn);
}
#endif

void CClassInfoHelperBase::SetCreateFunction(CClassTypeInfo* info,
                                             TCreateFunction func)
{
    info->SetCreateFunction(func);
}

void CClassInfoHelperBase::SetPreWriteFunction(CClassTypeInfo* info,
                                               TPreWriteFunction func)
{
    info->SetPreWriteFunction(func);
}

void CClassInfoHelperBase::SetPostReadFunction(CClassTypeInfo* info,
                                               TPostReadFunction func)
{
    info->SetPostReadFunction(func);
}

void CClassInfoHelperBase::UpdateCObject(CClassTypeInfo* info,
                                         const CObject* /*object*/)
{
    info->SetIsCObject();
}

void CClassInfoHelperBase::SetPreWriteFunction(CChoiceTypeInfo* info,
                                               TPreWriteFunction func)
{
    info->SetPreWriteFunction(func);
}

void CClassInfoHelperBase::SetPostReadFunction(CChoiceTypeInfo* info,
                                               TPostReadFunction func)
{
    info->SetPostReadFunction(func);
}

void CClassInfoHelperBase::UpdateCObject(CChoiceTypeInfo* info,
                                         const CObject* /*object*/)
{
    info->SetIsCObject();
}

END_NCBI_SCOPE
