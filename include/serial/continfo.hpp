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

BEGIN_NCBI_SCOPE

class CConstContainerElementIterator;
class CContainerElementIterator;

class CContainerTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    CContainerTypeInfo(bool randomOrder)
        : m_RandomOrder(randomOrder)
        {
        }
    CContainerTypeInfo(const string& name, bool randomOrder)
        : CParent(name), m_RandomOrder(randomOrder)
        {
        }
    CContainerTypeInfo(const char* name, bool randomOrder)
        : CParent(name), m_RandomOrder(randomOrder)
        {
        }
    virtual ETypeFamily GetTypeFamily(void) const;

    virtual TTypeInfo GetElementType(void) const = 0;

    bool RandomElementsOrder(void) const
        {
            return m_RandomOrder;
        }
    
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

    void Assign(TObjectPtr dst, TConstObjectPtr src) const;
    bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;

    virtual void AddElement(TObjectPtr containerPtr,
                            TConstObjectPtr elementPtr) const = 0;
    virtual void AddElement(TObjectPtr containerPtr,
                            CObjectIStream& in) const = 0;

protected:
    void ThrowDuplicateElementError(void) const;

    void ReadData(CObjectIStream& in, TObjectPtr containerPtr) const;
    void SkipData(CObjectIStream& in) const;
    void WriteData(CObjectOStream& out, TConstObjectPtr containerPtr) const;
    void CopyData(CObjectStreamCopier& copier) const;

private:
    bool m_RandomOrder;
};

class CContainerElementIterator
{
public:
    typedef CContainerTypeInfo::CIterator TIterator;

    CContainerElementIterator(void)
        : m_ElementType(0), m_Valid(false)
        {
        }
    CContainerElementIterator(TObjectPtr containerPtr,
                              const CContainerTypeInfo* containerType)
        : m_ElementType(containerType->GetElementType()),
          m_Iterator(containerType->NewIterator()),
          m_Valid(m_Iterator->Init(containerPtr))
        {
        }
    CContainerElementIterator(const CContainerElementIterator& src)
        : m_ElementType(src.m_ElementType),
          m_Iterator(src.CloneIterator()),
          m_Valid(src.m_Valid)
        {
        }

    CContainerElementIterator& operator=(const CContainerElementIterator& src)
        {
            m_Valid = false;
            m_ElementType = src.m_ElementType;
            m_Iterator = src.CloneIterator();
            m_Valid = src.m_Valid;
            return *this;
        }
    
    void Init(TObjectPtr containerPtr,
              const CContainerTypeInfo* containerType)
        {
            m_Valid = false;
            m_ElementType = containerType->GetElementType();
            m_Iterator.reset(containerType->NewIterator());
            m_Valid = m_Iterator->Init(containerPtr);
        }

    TTypeInfo GetElementType(void) const
        {
            return m_ElementType;
        }
    
    bool Valid(void) const
        {
            return m_Valid;
        }
    void Next(void)
        {
            _ASSERT(m_Valid);
            m_Valid = m_Iterator->Next();
        }
    void Erase(void)
        {
            _ASSERT(m_Valid);
            m_Valid = m_Iterator->Erase();
        }

    pair<TObjectPtr, TTypeInfo> Get(void) const
        {
            _ASSERT(m_Valid);
            return make_pair(m_Iterator->GetElementPtr(), GetElementType());
        }

private:
    TIterator* CloneIterator(void) const
        {
            TIterator* i = m_Iterator.get();
            return i? i->Clone(): 0;
        }

    TTypeInfo m_ElementType;
    AutoPtr<TIterator> m_Iterator;
    bool m_Valid;
};

class CConstContainerElementIterator
{
public:
    typedef CContainerTypeInfo::CConstIterator TIterator;

    CConstContainerElementIterator(void)
        : m_ElementType(0), m_Valid(false)
        {
        }
    CConstContainerElementIterator(TConstObjectPtr containerPtr,
                                   const CContainerTypeInfo* containerType)
        : m_ElementType(containerType->GetElementType()),
          m_Iterator(containerType->NewConstIterator()),
          m_Valid(m_Iterator->Init(containerPtr))
        {
        }
    CConstContainerElementIterator(const CConstContainerElementIterator& src)
        : m_ElementType(src.m_ElementType),
          m_Iterator(src.CloneIterator()),
          m_Valid(src.m_Valid)
        {
        }

    CConstContainerElementIterator&
    operator=(const CConstContainerElementIterator& src)
        {
            m_Valid = false;
            m_ElementType = src.m_ElementType;
            m_Iterator.reset(src.CloneIterator());
            m_Valid = src.m_Valid;
            return *this;
        }
    
    void Init(TConstObjectPtr containerPtr,
              const CContainerTypeInfo* containerType)
        {
            m_Valid = false;
            m_ElementType = containerType->GetElementType();
            m_Iterator.reset(containerType->NewConstIterator());
            m_Valid = m_Iterator->Init(containerPtr);
        }

    TTypeInfo GetElementType(void) const
        {
            return m_ElementType;
        }
    
    bool Valid(void) const
        {
            return m_Valid;
        }
    void Next(void)
        {
            _ASSERT(m_Valid);
            m_Valid = m_Iterator->Next();
        }

    pair<TConstObjectPtr, TTypeInfo> Get(void) const
        {
            _ASSERT(m_Valid);
            return make_pair(m_Iterator->GetElementPtr(), GetElementType());
        }

private:
    TIterator* CloneIterator(void) const
        {
            TIterator* i = m_Iterator.get();
            return i? i->Clone(): 0;
        }

    TTypeInfo m_ElementType;
    AutoPtr<TIterator> m_Iterator;
    bool m_Valid;
};

//#include <serial/continfo.inl>

END_NCBI_SCOPE

#endif  /* CONTINFO__HPP */
