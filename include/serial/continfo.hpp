#ifndef CONTINFO__HPP
#define CONTINFO__HPP

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
* Revision 1.5  2000/09/18 20:00:00  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.4  2000/09/13 15:10:13  vasilche
* Fixed type detection in type iterators.
*
* Revision 1.3  2000/09/01 13:15:58  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.2  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.1  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CConstContainerElementIterator;
class CContainerElementIterator;

class CContainerTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    CContainerTypeInfo(size_t size,
                       TTypeInfo elementType, bool randomOrder);
    CContainerTypeInfo(size_t size,
                       const CTypeRef& elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const char* name,
                       TTypeInfo elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const char* name,
                       const CTypeRef& elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const string& name,
                       TTypeInfo elementType, bool randomOrder);
    CContainerTypeInfo(size_t size, const string& name,
                       const CTypeRef& elementType, bool randomOrder);

    TTypeInfo GetElementType(void) const;

    bool RandomElementsOrder(void) const;
    
    class CConstIterator
    {
    public:
        virtual ~CConstIterator(void);
        
        virtual bool Init(TConstObjectPtr containerPtr) = 0;
        virtual bool Next(void) = 0;
        
        virtual TConstObjectPtr GetElementPtr(void) const = 0;
        
        virtual CConstIterator* Clone(void) const = 0;
    };
    
    class CIterator
    {
    public:
        virtual ~CIterator(void);
        
        virtual bool Init(TObjectPtr containerPtr) = 0;
        virtual bool Next(void) = 0;
        
        virtual TObjectPtr GetElementPtr(void) const = 0;
        
        virtual CIterator* Clone(void) const = 0;
        
        virtual bool Erase(void) = 0;
    };

    virtual CConstIterator* NewConstIterator(void) const = 0;
    virtual CIterator* NewIterator(void) const;

    virtual bool MayContainType(TTypeInfo type) const;

    void Assign(TObjectPtr dst, TConstObjectPtr src) const;
    bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;

    virtual void AddElement(TObjectPtr containerPtr,
                            TConstObjectPtr elementPtr) const = 0;
    virtual void AddElement(TObjectPtr containerPtr,
                            CObjectIStream& in) const = 0;

protected:
    void ThrowDuplicateElementError(void) const;

    static void ReadContainer(CObjectIStream& in,
                              TTypeInfo objectType,
                              TObjectPtr objectPtr);
    static void WriteContainer(CObjectOStream& out,
                               TTypeInfo objectType,
                               TConstObjectPtr objectPtr);
    static void SkipContainer(CObjectIStream& in,
                              TTypeInfo objectType);
    static void CopyContainer(CObjectStreamCopier& copier,
                              TTypeInfo objectType);

protected:
    CTypeRef m_ElementType;
    bool m_RandomOrder;

private:
    void InitContainerTypeInfoFunctions(void);
};

class CContainerElementIterator
{
public:
    typedef CContainerTypeInfo::CIterator TIterator;

    CContainerElementIterator(void);
    CContainerElementIterator(TObjectPtr containerPtr,
                              const CContainerTypeInfo* containerType);
    CContainerElementIterator(const CContainerElementIterator& src);
    CContainerElementIterator& operator=(const CContainerElementIterator& src);
    void Init(TObjectPtr containerPtr,
              const CContainerTypeInfo* containerType);

    TTypeInfo GetElementType(void) const;
    
    bool Valid(void) const;
    void Next(void);
    void Erase(void);

    pair<TObjectPtr, TTypeInfo> Get(void) const;

private:
    TIterator* CloneIterator(void) const;

    TTypeInfo m_ElementType;
    AutoPtr<TIterator> m_Iterator;
    bool m_Valid;
};

class CConstContainerElementIterator
{
public:
    typedef CContainerTypeInfo::CConstIterator TIterator;

    CConstContainerElementIterator(void);
    CConstContainerElementIterator(TConstObjectPtr containerPtr,
                                   const CContainerTypeInfo* containerType);
    CConstContainerElementIterator(const CConstContainerElementIterator& src);

    CConstContainerElementIterator&
    operator=(const CConstContainerElementIterator& src);
    
    void Init(TConstObjectPtr containerPtr,
              const CContainerTypeInfo* containerType);

    TTypeInfo GetElementType(void) const;
    
    bool Valid(void) const;
    void Next(void);

    pair<TConstObjectPtr, TTypeInfo> Get(void) const;

private:
    TIterator* CloneIterator(void) const;

    TTypeInfo m_ElementType;
    AutoPtr<TIterator> m_Iterator;
    bool m_Valid;
};

#include <serial/continfo.inl>

END_NCBI_SCOPE

#endif  /* CONTINFO__HPP */
