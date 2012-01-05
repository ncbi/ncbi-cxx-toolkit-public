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
*   Read and write hooks
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <serial/impl/objecttype.hpp>
#include <serial/impl/objstack.hpp>
#include <serial/objectiter.hpp>


/** @addtogroup ObjStreamSupport
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

/// Read hook for a standalone object
class NCBI_XSERIAL_EXPORT CReadObjectHook : public CObject
{
public:
    virtual ~CReadObjectHook(void);
    
    /// This method will be called at approriate time
    /// when the object of requested type is to be read
    virtual void ReadObject(CObjectIStream& in,
                            const CObjectInfo& object) = 0;
    // Default actions
    /// Default read
    void DefaultRead(CObjectIStream& in,
                     const CObjectInfo& object);
    /// Default skip
    void DefaultSkip(CObjectIStream& in,
                     const CObjectInfo& object);
};

/// Read hook for data member of a containing object (eg, SEQUENCE)
class NCBI_XSERIAL_EXPORT CReadClassMemberHook : public CObject
{
public:
    virtual ~CReadClassMemberHook(void);

    /// This method will be called at approriate time
    /// when the object of requested type is to be read
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

/// Read hook for data member of a containing object (eg, SEQUENCE)
class NCBI_XSERIAL_EXPORT CPreReadClassMemberHook
    : public CReadClassMemberHook
{
public:
    virtual ~CPreReadClassMemberHook(void);

    /// This method will be called at approriate time
    /// when the object of requested type is to be read
    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& member);

    /// Return true to invoke default reading method afterwards.
    /// Return false if no firther reading needs to be done.
    virtual void PreReadClassMember(CObjectIStream& in,
                                    const CObjectInfoMI& member) = 0;
};

/// Read hook for a choice variant (CHOICE)
class NCBI_XSERIAL_EXPORT CReadChoiceVariantHook : public CObject
{
public:
    virtual ~CReadChoiceVariantHook(void);

    /// This method will be called at approriate time
    /// when the object of requested type is to be read
    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CObjectInfoCV& variant) = 0;
    void DefaultRead(CObjectIStream& in,
                     const CObjectInfoCV& object);
    // No default skip method - can not skip variants
};

/// Read hook for a choice variant (CHOICE)
class NCBI_XSERIAL_EXPORT CPreReadChoiceVariantHook
    : public CReadChoiceVariantHook
{
public:
    virtual ~CPreReadChoiceVariantHook(void);

    /// This method will be called at approriate time
    /// when the object of requested type is to be read
    virtual void ReadChoiceVariant(CObjectIStream& in,
                                   const CObjectInfoCV& variant);

    /// Return true to invoke default reading method afterwards.
    /// Return false if no firther reading needs to be done.
    virtual void PreReadChoiceVariant(CObjectIStream& in,
                                      const CObjectInfoCV& object) = 0;
};

/// Read hook for a container element (SEQUENCE OF)
class NCBI_XSERIAL_EXPORT CReadContainerElementHook : public CObject
{
public:
    virtual ~CReadContainerElementHook(void);

    virtual void ReadContainerElement(CObjectIStream& in,
                                      const CObjectInfo& container) = 0;
};

/// Write hook for a standalone object
class NCBI_XSERIAL_EXPORT CWriteObjectHook : public CObject
{
public:
    virtual ~CWriteObjectHook(void);
    
    /// This method will be called at approriate time
    /// when the object of requested type is to be written
    virtual void WriteObject(CObjectOStream& out,
                             const CConstObjectInfo& object) = 0;
    void DefaultWrite(CObjectOStream& out,
                      const CConstObjectInfo& object);
};

/// Write hook for data member of a containing object (eg, SEQUENCE)
class NCBI_XSERIAL_EXPORT CWriteClassMemberHook : public CObject
{
public:
    virtual ~CWriteClassMemberHook(void);
    
    virtual void WriteClassMember(CObjectOStream& out,
                                  const CConstObjectInfoMI& member) = 0;
    void DefaultWrite(CObjectOStream& out,
                      const CConstObjectInfoMI& member);
};

/// Write hook for a choice variant (CHOICE)
class NCBI_XSERIAL_EXPORT CWriteChoiceVariantHook : public CObject
{
public:
    virtual ~CWriteChoiceVariantHook(void);

    virtual void WriteChoiceVariant(CObjectOStream& out,
                                    const CConstObjectInfoCV& variant) = 0;
    void DefaultWrite(CObjectOStream& out,
                      const CConstObjectInfoCV& variant);
};

/// Skip hook for a standalone object
class NCBI_XSERIAL_EXPORT CSkipObjectHook : public CObject
{
public:
    virtual ~CSkipObjectHook(void);
    
    virtual void SkipObject(CObjectIStream& stream,
                            const CObjectTypeInfo& type) = 0;

    // Default actions
    /// Default read
    void DefaultRead(CObjectIStream& in,
                     const CObjectInfo& object);
    /// Default skip
    void DefaultSkip(CObjectIStream& in,
                     const CObjectTypeInfo& type);
};

/// Skip hook for data member of a containing object (eg, SEQUENCE)
class NCBI_XSERIAL_EXPORT CSkipClassMemberHook : public CObject
{
public:
    virtual ~CSkipClassMemberHook(void);
    
    virtual void SkipClassMember(CObjectIStream& stream,
                                 const CObjectTypeInfoMI& member) = 0;
    virtual void SkipMissingClassMember(CObjectIStream& stream,
                                        const CObjectTypeInfoMI& member);
    void DefaultSkip(CObjectIStream& stream,
                     const CObjectTypeInfoMI& member);
};

/// Skip hook for a choice variant (CHOICE)
class NCBI_XSERIAL_EXPORT CSkipChoiceVariantHook : public CObject
{
public:
    virtual ~CSkipChoiceVariantHook(void);

    virtual void SkipChoiceVariant(CObjectIStream& stream,
                                   const CObjectTypeInfoCV& variant) = 0;
//    void DefaultSkip(CObjectIStream& stream,
//                     const CObjectTypeInfoCV& variant);
};


/// Copy hook for a standalone object
class NCBI_XSERIAL_EXPORT CCopyObjectHook : public CObject
{
public:
    virtual ~CCopyObjectHook(void);
    
    virtual void CopyObject(CObjectStreamCopier& copier,
                            const CObjectTypeInfo& type) = 0;
    void DefaultCopy(CObjectStreamCopier& copier,
                     const CObjectTypeInfo& type);
};

/// Copy hook for data member of a containing object (eg, SEQUENCE)
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

/// Copy hook for a choice variant (CHOICE)
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


/// Helper class: installs hooks in constructor, and uninstalls in destructor
template <class T>
class CObjectHookGuard : public CObjectHookGuardBase
{
    typedef CObjectHookGuardBase CParent;
public:
    /// Install object read hook
    ///
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(CReadObjectHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }
    /// Install object write hook
    ///
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(CWriteObjectHook& hook,
                     CObjectOStream* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }
    /// Install object skip hook
    ///
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(CSkipObjectHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }
    /// Install object copy hook
    ///
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(CCopyObjectHook& hook,
                     CObjectStreamCopier* stream = 0)
        : CParent(CType<T>(), hook, stream)
        {
        }

    /// Install member read hook
    ///
    /// @param id
    ///   Member id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CReadClassMemberHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install member write hook
    ///
    /// @param id
    ///   Member id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CWriteClassMemberHook& hook,
                     CObjectOStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install member skip hook
    ///
    /// @param id
    ///   Member id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CSkipClassMemberHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install member copy hook
    ///
    /// @param id
    ///   Member id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CCopyClassMemberHook& hook,
                     CObjectStreamCopier* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install choice variant read hook
    ///
    /// @param id
    ///   Variant id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CReadChoiceVariantHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install choice variant write hook
    ///
    /// @param id
    ///   Variant id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CWriteChoiceVariantHook& hook,
                     CObjectOStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install choice variant skip hook
    ///
    /// @param id
    ///   Variant id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
    CObjectHookGuard(const string& id,
                     CSkipChoiceVariantHook& hook,
                     CObjectIStream* stream = 0)
        : CParent(CType<T>(), id, hook, stream)
        {
        }

    /// Install choice variant copy hook
    ///
    /// @param id
    ///   Variant id
    /// @param hook
    ///   Hook object
    /// @param stream
    ///   Data stream:  if 0, the global hook is installed,
    ///   otherwise - local one
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


/// Helper hook for Serial_FilterObjects function template;
/// User hook class should be derived from this base class
template<typename TObject>
class CSerial_FilterObjectsHook : public CSkipObjectHook
{
public:
    void SkipObject(CObjectIStream& in, const CObjectTypeInfo& type)
    {
        TObject obj;
        type.GetTypeInfo()->DefaultReadData(in, &obj);
        Process(obj);
    }
    /// This method will be called when the object of the
    /// requested class is read
    virtual void Process(const TObject& obj) = 0;
};

template<typename TObject>
class CSerial_FilterReadObjectsHook : public CReadObjectHook
{
public:
    CSerial_FilterReadObjectsHook<TObject>(
        CSerial_FilterObjectsHook<TObject>* processor)
        : m_processor(processor)
    {
    }
    void ReadObject(CObjectIStream& in,const CObjectInfo& object)
    {
        DefaultRead(in,object);
        TObject* obj = (TObject*)(object.GetObjectPtr()); 
        m_processor->Process(*obj);
    }
private:
    CSerial_FilterObjectsHook<TObject>* m_processor;
};

NCBI_XSERIAL_EXPORT
void Serial_FilterSkip(CObjectIStream& in, CObjectTypeInfo& ctype);

class CEofException;
/// Scan input stream, finding objects of requested type (TObject) only
template<typename TRoot, typename TObject>
void Serial_FilterObjects(CObjectIStream& in, CSerial_FilterObjectsHook<TObject>* hook,
                          bool readall=true)
{
    CObjectTypeInfo root = CType<TRoot>();
    CObjectTypeInfo request = CType<TObject>();
    request.SetLocalSkipHook(in, hook);
    request.SetLocalReadHook(in, new CSerial_FilterReadObjectsHook<TObject>(hook));
    do {
        try {
            Serial_FilterSkip(in,root);
        } catch ( CEofException& ) {
            return;
        }
    } while (readall);
}

/// Scan input stream, finding objects that are not derived from CSerialObject
template<typename TRoot, typename TObject>
void Serial_FilterStdObjects(CObjectIStream& in, CSerial_FilterObjectsHook<TObject>* hook,
                          bool readall=true)
{
    CObjectTypeInfo root = CType<TRoot>();
    CObjectTypeInfo request = CStdTypeInfo<TObject>::GetTypeInfo();
    request.SetLocalSkipHook(in, hook);
    do {
        try {
            Serial_FilterSkip(in,root);
        } catch ( CEofException& ) {
            return;
        }
    } while (readall);
}




/* @} */


END_NCBI_SCOPE

#endif  /* OBJHOOK__HPP */
