#ifndef OBJHOOK__HPP
#define OBJHOOK__HPP

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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <serial/objecttype.hpp>
#include <serial/objstack.hpp>
#include <serial/objectiter.hpp>


/** @addtogroup HookSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CObjectStreamCopier;
class CObjectInfo;
class CConstObjectInfo;
class CObjectTypeInfo;

class NCBI_XSERIAL_EXPORT CReadObjectHook : public CObject
{
public:
    virtual ~CReadObjectHook(void);
    
    virtual void ReadObject(CObjectIStream& in,
                            const CObjectInfo& object) = 0;
    // Default actions
    void DefaultRead(CObjectIStream& in,
                     const CObjectInfo& object);
    void DefaultSkip(CObjectIStream& in,
                     const CObjectInfo& object);
};

class NCBI_XSERIAL_EXPORT CReadClassMemberHook : public CObject
{
public:
    virtual ~CReadClassMemberHook(void);

    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& member) = 0;
    virtual void ReadMissingClassMember(CObjectIStream& in,
                                        const CObjectInfoMI& member);
    void DefaultRead(CObjectIStream& in,
                     const CObjectInfoMI& object);
    void DefaultSkip(CObjectIStream& in,
                     const CObjectInfoMI& object);
    void ResetMember(const CObjectInfoMI& object,
                     CObjectInfoMI::EEraseFlag flag =
                         CObjectInfoMI::eErase_Optional);
};

class NCBI_XSERIAL_EXPORT CReadChoiceVariantHook : public CObject
{
public:
    virtual ~CReadChoiceVariantHook(void);

    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CObjectInfoCV& variant) = 0;
    void DefaultRead(CObjectIStream& in,
                     const CObjectInfoCV& object);
    // No default skip method - can not skip variants
};

class NCBI_XSERIAL_EXPORT CReadContainerElementHook : public CObject
{
public:
    virtual ~CReadContainerElementHook(void);

    virtual void ReadContainerElement(CObjectIStream& in,
                                      const CObjectInfo& container) = 0;
};

class NCBI_XSERIAL_EXPORT CWriteObjectHook : public CObject
{
public:
    virtual ~CWriteObjectHook(void);
    
    virtual void WriteObject(CObjectOStream& out,
                             const CConstObjectInfo& object) = 0;
    void DefaultWrite(CObjectOStream& out,
                      const CConstObjectInfo& object);
};

class NCBI_XSERIAL_EXPORT CWriteClassMemberHook : public CObject
{
public:
    virtual ~CWriteClassMemberHook(void);
    
    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfoMI& member) = 0;
    void DefaultWrite(CObjectOStream& out,
                      const CConstObjectInfoMI& member);
};

class NCBI_XSERIAL_EXPORT CWriteChoiceVariantHook : public CObject
{
public:
    virtual ~CWriteChoiceVariantHook(void);

    virtual void WriteChoiceVariant(CObjectOStream& out,
                                    const CConstObjectInfoCV& variant) = 0;
    void DefaultWrite(CObjectOStream& out,
                      const CConstObjectInfoCV& variant);
};

class NCBI_XSERIAL_EXPORT CSkipObjectHook : public CObject
{
public:
    virtual ~CSkipObjectHook(void);
    
    virtual void SkipObject(CObjectIStream& stream,
                            const CObjectTypeInfo& type) = 0;
//    void DefaultSkip(CObjectIStream& stream,
//                     const CObjectTypeInfo& type);
};

class NCBI_XSERIAL_EXPORT CSkipClassMemberHook : public CObject
{
public:
    virtual ~CSkipClassMemberHook(void);
    
    virtual void SkipClassMember(CObjectIStream& stream,
                                 const CObjectTypeInfoMI& member) = 0;
    virtual void SkipMissingClassMember(CObjectIStream& stream,
                                        const CObjectTypeInfoMI& member);
//    void DefaultSkip(CObjectIStream& stream,
//                     const CObjectTypeInfoMI& member);
};

class NCBI_XSERIAL_EXPORT CSkipChoiceVariantHook : public CObject
{
public:
    virtual ~CSkipChoiceVariantHook(void);

    virtual void SkipChoiceVariant(CObjectIStream& stream,
                                   const CObjectTypeInfoCV& variant) = 0;
//    void DefaultSkip(CObjectIStream& stream,
//                     const CObjectTypeInfoCV& variant);
};


class NCBI_XSERIAL_EXPORT CCopyObjectHook : public CObject
{
public:
    virtual ~CCopyObjectHook(void);
    
    virtual void CopyObject(CObjectStreamCopier& copier,
                            const CObjectTypeInfo& type) = 0;
    void DefaultCopy(CObjectStreamCopier& copier,
                     const CObjectTypeInfo& type);
};

class NCBI_XSERIAL_EXPORT CCopyClassMemberHook : public CObject
{
public:
    virtual ~CCopyClassMemberHook(void);
    
    virtual void CopyClassMember(CObjectStreamCopier& copier,
                                 const CObjectTypeInfoMI& member) = 0;
    virtual void CopyMissingClassMember(CObjectStreamCopier& copier,
                                        const CObjectTypeInfoMI& member);
    void DefaultCopy(CObjectStreamCopier& copier,
                     const CObjectTypeInfoMI& member);
};

class NCBI_XSERIAL_EXPORT CCopyChoiceVariantHook : public CObject
{
public:
    virtual ~CCopyChoiceVariantHook(void);

    virtual void CopyChoiceVariant(CObjectStreamCopier& copier,
                                   const CObjectTypeInfoCV& variant) = 0;
    void DefaultCopy(CObjectStreamCopier& copier,
                     const CObjectTypeInfoCV& variant);
};


enum EDefaultHookAction {
    eDefault_Normal,        // read or write data
    eDefault_Skip           // skip data
};


class NCBI_XSERIAL_EXPORT CObjectHookGuardBase
{
protected:
    // object read hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         CReadObjectHook& hook,
                         CObjectIStream* stream = 0);
    // object write hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         CWriteObjectHook& hook,
                         CObjectOStream* stream = 0);
    // object skip hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         CSkipObjectHook& hook,
                         CObjectIStream* stream = 0);
    // object copy hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         CCopyObjectHook& hook,
                         CObjectStreamCopier* stream = 0);

    // member read hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CReadClassMemberHook& hook,
                         CObjectIStream* stream = 0);
    // member write hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CWriteClassMemberHook& hook,
                         CObjectOStream* stream = 0);
    // member skip hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CSkipClassMemberHook& hook,
                         CObjectIStream* stream = 0);
    // member copy hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CCopyClassMemberHook& hook,
                         CObjectStreamCopier* stream = 0);

    // choice variant read hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CReadChoiceVariantHook& hook,
                         CObjectIStream* stream = 0);
    // choice variant write hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CWriteChoiceVariantHook& hook,
                         CObjectOStream* stream = 0);
    // choice variant skip hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CSkipChoiceVariantHook& hook,
                         CObjectIStream* stream = 0);
    // choice variant copy hook
    CObjectHookGuardBase(const CObjectTypeInfo& info,
                         const string& id,
                         CCopyChoiceVariantHook& hook,
                         CObjectStreamCopier* stream = 0);

    ~CObjectHookGuardBase(void);

    void ResetHook(const CObjectTypeInfo& info);

private:
    CObjectHookGuardBase(const CObjectHookGuardBase&);
    const CObjectHookGuardBase& operator=(const CObjectHookGuardBase&);

    enum EHookMode {
        eHook_None,
        eHook_Read,
        eHook_Write,
        eHook_Skip,
        eHook_Copy
    };
    enum EHookType {
        eHook_Null,         // object hook
        eHook_Object,       // object hook
        eHook_Member,       // class member hook
        eHook_Variant,      // choice variant hook
        eHook_Element       // container element hook
    };

    union {
        CObjectIStream*      m_IStream;
        CObjectOStream*      m_OStream;
        CObjectStreamCopier* m_Copier;
    } m_Stream;
    CRef<CObject> m_Hook;
    EHookMode m_HookMode;
    EHookType m_HookType;
    string m_Id;            // member or variant id
};


template <class T>
class CObjectHookGuard : CObjectHookGuardBase
{
    typedef CObjectHookGuardBase CParent;
public:
    // object read hook
    CObjectHookGuard(CReadObjectHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }
    // object write hook
    CObjectHookGuard(CWriteObjectHook& hook,
                     CObjectOStream* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }
    // object skip hook
    CObjectHookGuard(CSkipObjectHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }
    // object copy hook
    CObjectHookGuard(CCopyObjectHook& hook,
                     CObjectStreamCopier* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }

    // member read hook
    CObjectHookGuard(const string& id,
                     CReadClassMemberHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }
    // member write hook
    CObjectHookGuard(const string& id,
                     CWriteClassMemberHook& hook,
                     CObjectOStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }
    // member skip hook
    CObjectHookGuard(const string& id,
                     CSkipClassMemberHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }
    // member copy hook
    CObjectHookGuard(const string& id,
                     CCopyClassMemberHook& hook,
                     CObjectStreamCopier* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    // choice variant read hook
    CObjectHookGuard(const string& id,
                     CReadChoiceVariantHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }
    // choice variant write hook
    CObjectHookGuard(const string& id,
                     CWriteChoiceVariantHook& hook,
                     CObjectOStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }
    // choice variant skip hook
    CObjectHookGuard(const string& id,
                     CSkipChoiceVariantHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }
    // choice variant copy hook
    CObjectHookGuard(const string& id,
                     CCopyChoiceVariantHook& hook,
                     CObjectStreamCopier* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    ~CObjectHookGuard(void)
        {
            CParent::ResetHook(CType<T>());
        }
};


/* @} */


//#include <serial/objhook.inl>

END_NCBI_SCOPE

#endif  /* OBJHOOK__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2004/04/30 13:28:40  gouriano
* Remove obsolete function declarations
*
* Revision 1.14  2003/08/11 15:25:50  grichenk
* Added possibility to reset an object member from
* a read hook (including non-optional members).
*
* Revision 1.13  2003/07/29 18:47:46  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.12  2003/04/15 16:18:18  siyan
* Added doxygen support
*
* Revision 1.11  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.10  2002/09/19 14:00:37  grichenk
* Implemented CObjectHookGuard for write and copy hooks
* Added DefaultRead/Write/Copy methods to base hook classes
*
* Revision 1.8  2002/09/09 18:44:53  grichenk
* Fixed CObjectHookGuard methods for GCC
*
* Revision 1.7  2002/09/09 18:20:18  grichenk
* Fixed streamcludes (to declare Type<>)
*
* Revision 1.6  2002/09/09 18:13:59  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.5  2000/11/01 20:35:27  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.4  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation stream streams.
* Moved streamstantiation of several templates stream separate source file.
*
* Revision 1.3  2000/09/26 17:38:07  vasilche
* Fixed streamcomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.2  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored stream CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/08/15 19:44:39  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* ===========================================================================
*/
