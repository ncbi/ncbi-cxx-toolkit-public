#ifndef NCBIREF__HPP
#define NCBIREF__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Standard CObject and CRef classes for reference counter based GC
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.10  2000/06/16 16:29:42  vasilche
* Added SetCanDelete() method to allow to change CObject 'in heap' status immediately after creation.
*
* Revision 1.9  2000/06/07 19:44:16  vasilche
* Removed unneeded THROWS declaration - they lead to encreased code size.
*
* Revision 1.8  2000/05/09 16:36:54  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
*
* Revision 1.7  2000/04/28 16:56:13  vasilche
* Fixed implementation of CRef<> and CConstRef<>
*
* Revision 1.6  2000/03/31 17:08:07  kans
* moved ECanDelete to public area of CObject
*
* Revision 1.5  2000/03/29 15:50:27  vasilche
* Added const version of CRef - CConstRef.
* CRef and CConstRef now accept classes inherited from CObject.
*
* Revision 1.4  2000/03/10 14:18:37  vasilche
* Added CRef<>::GetPointerOrNull() method similar to std::auto_ptr<>.get()
*
* Revision 1.3  2000/03/08 14:18:19  vasilche
* Fixed throws instructions.
*
* Revision 1.2  2000/03/07 15:25:42  vasilche
* Fixed implementation of CRef::->
*
* Revision 1.1  2000/03/07 14:03:11  vasilche
* Added CObject class as base for reference counted objects.
* Added CRef templace for reference to CObject descendant.
*
*
* ==========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

class CNullPointerError : public exception
{
public:
    CNullPointerError(void) THROWS_NONE;
    ~CNullPointerError(void) THROWS_NONE;

    const char* what() const THROWS_NONE;
};


class CObject
{
private:
    // special flag in counter meaning that object is not allocated in heap
    enum {
        eDoNotDelete = int(0x80000000u)
    };

public:
    // main constructor for static/automatic/enclosed objects
    CObject(void) THROWS_NONE
        : m_Counter(eDoNotDelete)
        {
        }
    // virtual destructor
    virtual ~CObject(void);

    enum ECanDelete {
        eCanDelete = 0
    };

protected:
    // special constructor for objects allocated in heap
    CObject(ECanDelete) THROWS_NONE
        : m_Counter(0)
        {
        }

public:
    inline
    bool CanBeDeleted(void) const THROWS_NONE
        {
            return (m_Counter & eDoNotDelete) == 0;
        }

    inline
    unsigned ReferenceCount(void) const THROWS_NONE
        {
            return (m_Counter & ~eDoNotDelete);
        }

    inline
    bool Referenced(void) const THROWS_NONE
        {
            return ReferenceCount() != 0;
        }

    inline
    void AddReference(void) const THROWS_NONE
        {
            ++m_Counter;
        }

    inline
    void RemoveReference(void) const
        {
            if ( unsigned(m_Counter-- & ~eDoNotDelete) <= 1 ) {
                RemoveLastReference();
            }
        }

    // remove reference without deleting object
    void ReleaseReference(void) const;

    // set flag eCanDelete meaning that object was allocated in heap
    void SetCanDelete(void)
        {
            _ASSERT(m_Counter == unsigned(eDoNotDelete));
            m_Counter = 0;
        }

private:
    void RemoveLastReference(void) const;

    mutable unsigned m_Counter;
};

template<class C>
class CRefBase
{
public:
    static
    void AddReference(const C* object)
        {
            object->AddReference();
        }

    static
    void RemoveReference(const C* object)
        {
            object->RemoveReference();
        }
    
    static
    void ReleaseReference(const C* object)
        {
            object->ReleaseReference();
        }
};

template<class C>
class CRef {
public:
    typedef C TObjectType;

    inline
    CRef(void) THROWS_NONE
        : m_Ptr(0)
        {
        }
    CRef(TObjectType* ptr)
        {
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    CRef(const CRef<C>& ref)
        {
            TObjectType* ptr = ref.m_Ptr;
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    ~CRef(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                CRefBase<C>::RemoveReference(ptr);
        }
    
    // test
    bool Empty(void) const THROWS_NONE
        {
            return m_Ptr == 0;
        }
    bool NotEmpty(void) const THROWS_NONE
        {
            return m_Ptr != 0;
        }

    // test
    operator bool(void) THROWS_NONE
        {
            return NotEmpty();
        }
    operator bool(void) const THROWS_NONE
        {
            return NotEmpty();
        }
    bool operator!(void) const THROWS_NONE
        {
            return Empty();
        }

    // reset
    inline
    void Reset(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr ) {
                CRefBase<C>::RemoveReference(ptr);
                m_Ptr = 0;
            }
        }
    inline
    void Reset(TObjectType* newPtr)
        {
            TObjectType* oldPtr = m_Ptr;
            if ( newPtr != oldPtr ) {
                if ( oldPtr )
                    CRefBase<C>::RemoveReference(oldPtr);
                if ( newPtr )
                    CRefBase<C>::AddReference(newPtr);
                m_Ptr = newPtr;
            }
        }

    // release
    inline
    TObjectType* ReleaseOrNull(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                return 0;
            CRefBase<C>::ReleaseReference(ptr);
            m_Ptr = 0;
            return ptr;
        }
    inline
    TObjectType* Release(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            CRefBase<C>::ReleaseReference(ptr);
            m_Ptr = 0;
            return ptr;
        }

    // assign
    CRef<C>& operator=(const CRef<C>& ref)
        {
            Reset(ref.m_Ptr);
            return *this;
        }


    // getters
    inline
    TObjectType* GetPointerOrNull(void) const THROWS_NONE
        {
            return m_Ptr;
        }
    inline
    TObjectType* GetPointer(void) const
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return ptr;
        }
    inline
    TObjectType& GetObject(void) const
        {
            return *GetPointer();
        }

    inline
    const TObjectType& operator*(void) const
        {
            return GetObject();
        }
    inline
    TObjectType& operator*(void)
        {
            return GetObject();
        }
    inline
    const TObjectType* operator->(void) const
        {
            return GetPointer();
        }
    inline
    TObjectType* operator->(void)
        {
            return GetPointer();
        }

private:
    TObjectType* m_Ptr;
};

template<class C>
class CConstRef {
public:
    typedef const C TObjectType;

    inline
    CConstRef(void) THROWS_NONE
        : m_Ptr(0)
        {
        }
    CConstRef(TObjectType* ptr)
        {
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    CConstRef(const CConstRef<C>& ref)
        {
            TObjectType* ptr = ref.m_Ptr;
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    CConstRef(const CRef<C>& ref)
        {
            TObjectType* ptr = ref.GetPointerOrNull();
            if ( ptr )
                CRefBase<C>::AddReference(ptr);
            m_Ptr = ptr;
        }
    ~CConstRef(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                CRefBase<C>::RemoveReference(ptr);
        }
    
    // test
    bool Empty(void) const THROWS_NONE
        {
            return m_Ptr == 0;
        }
    bool NotEmpty(void) const THROWS_NONE
        {
            return m_Ptr != 0;
        }

    // test
    operator bool(void) THROWS_NONE
        {
            return NotEmpty();
        }
    operator bool(void) const THROWS_NONE
        {
            return NotEmpty();
        }
    bool operator!(void) const THROWS_NONE
        {
            return Empty();
        }

    // reset
    inline
    void Reset(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr ) {
                CRefBase<C>::RemoveReference(ptr);
                m_Ptr = 0;
            }
        }
    inline
    void Reset(TObjectType* newPtr)
        {
            TObjectType* oldPtr = m_Ptr;
            if ( newPtr != oldPtr ) {
                if ( oldPtr )
                    CRefBase<C>::RemoveReference(oldPtr);
                if ( newPtr )
                    CRefBase<C>::AddReference(newPtr);
                m_Ptr = newPtr;
            }
        }

    // release
    inline
    TObjectType* ReleaseOrNull(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                return 0;
            CRefBase<C>::ReleaseReference(ptr);
            m_Ptr = 0;
            return ptr;
        }
    inline
    TObjectType* Release(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            CRefBase<C>::ReleaseReference(ptr);
            m_Ptr = 0;
            return ptr;
        }

    // assign
    CConstRef<C>& operator=(const CConstRef<C>& ref)
        {
            Reset(ref.m_Ptr);
            return *this;
        }
    // assign
    CConstRef<C>& operator=(const CRef<C>& ref)
        {
            Reset(ref.GetPointerOrNull());
            return *this;
        }


    // getters
    inline
    TObjectType* GetPointerOrNull(void) const THROWS_NONE
        {
            return m_Ptr;
        }
    inline
    TObjectType* GetPointer(void) const
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return ptr;
        }
    inline
    TObjectType& GetObject(void) const
        {
            return *GetPointer();
        }

    inline
    TObjectType& operator*(void) const
        {
            return GetObject();
        }
    inline
    TObjectType* operator->(void) const
        {
            return GetPointer();
        }

private:
    TObjectType* m_Ptr;
};

template<typename T>
class CObjectFor : public CObject
{
public:
    typedef T TObjectType;

protected:
    CObjectFor(void)
        : CObject(eCanDelete)
        {
        }

public:
    // can be allocated only in heap via New();
    static CObjectFor<T>* New(void)
        {
            return new CObjectFor<T>();
        }

    T& GetData(void)
        {
            return m_Data;
        }
    const T& GetData(void) const
        {
            return m_Data;
        }
    operator T& (void)
        {
            return GetData();
        }
    operator const T& (void) const
        {
            return GetData();
        }

    T& operator=(const T& data)
        {
            m_Data = data;
            return *this;
        }

private:
    T m_Data;
};

END_NCBI_SCOPE

#endif
