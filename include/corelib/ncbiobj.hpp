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
    void ReleaseReference(void) const THROWS((runtime_error));

    static const CTypeInfo* GetTypeInfo(void);

private:
    void RemoveLastReference(void) const;

    mutable unsigned m_Counter;
};

class CRefBase {
public:
    typedef const CObject TObjectType;

    // constructor
    CRefBase(void) THROWS_NONE
        : m_Ptr(0)
        {
        }
    CRefBase(TObjectType* ptr) THROWS_NONE
        : m_Ptr(ptr)
        {
            if ( ptr )
                ptr->AddReference();
        }
    // destructor
    ~CRefBase(void)
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                ptr->RemoveReference();
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

protected:
    // assign
    void x_Reset(TObjectType* newPtr)
        {
            TObjectType* oldPtr = m_Ptr;
            if ( newPtr != oldPtr ) {
                if ( oldPtr )
                    oldPtr->RemoveReference();
                m_Ptr = newPtr;
                if ( newPtr )
                    newPtr->AddReference();
            }
        }

    // getters
    TObjectType* x_GetPointerOrNull(void) const THROWS_NONE
        {
            return m_Ptr;
        }
    TObjectType* x_GetPointer(void) const THROWS((CNullPointerError))
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return ptr;
        }
    TObjectType& x_GetObject(void) const THROWS((CNullPointerError))
        {
            return *x_GetPointer();
        }

    // release
    TObjectType* x_ReleaseOrNull(void) THROWS_NONE
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                ptr->ReleaseReference();
            return ptr;
        }
    TObjectType* x_Release(void) THROWS((CNullPointerError))
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            ptr->ReleaseReference();
            return ptr;
        }

public:
    CRefBase& operator=(const CRefBase& ref)
        {
            x_Reset(ref.x_GetPointerOrNull());
            return *this;
        }

private:
    TObjectType* m_Ptr;
};

template<class C>
class CRef : public CRefBase {
    typedef CRefBase CParent;
public:
    typedef C TObjectType;

    inline
    CRef(void) THROWS_NONE
        {
        }
    CRef(TObjectType* ptr) THROWS_NONE
        : CParent(ptr)
        {
        }
    CRef(const CRef<C>& ref) THROWS_NONE
        : CParent(ref.GetPointerOrNull())
        {
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

    // reset
    inline
    void Reset(TObjectType* ptr = 0)
        {
            x_Reset(ptr);
        }

    // getters
    inline
    const TObjectType* GetPointerOrNull(void) const THROWS_NONE
        {
            return static_cast<const TObjectType*>(x_GetPointerOrNull());
        }
    inline
    TObjectType* GetPointerOrNull(void) THROWS_NONE
        {
            return const_cast<TObjectType*>(static_cast<const TObjectType*>(x_GetPointerOrNull()));
        }
    inline
    const TObjectType* GetPointer(void) const THROWS((CNullPointerError))
        {
            return static_cast<const TObjectType*>(x_GetPointer());
        }
    inline
    TObjectType* GetPointer(void) THROWS((CNullPointerError))
        {
            return const_cast<TObjectType*>(static_cast<const TObjectType*>(x_GetPointer()));
        }
    inline
    const TObjectType& GetObject(void) const THROWS((CNullPointerError))
        {
            return *GetPointer();
        }
    inline
    TObjectType& GetObject(void) THROWS((CNullPointerError))
        {
            return *GetPointer();
        }
    inline
    const TObjectType& operator*(void) const THROWS((CNullPointerError))
        {
            return GetObject();
        }
    inline
    TObjectType& operator*(void) THROWS((CNullPointerError))
        {
            return GetObject();
        }
    inline
    const TObjectType* operator->(void) const THROWS((CNullPointerError))
        {
            return GetPointer();
        }
    inline
    TObjectType* operator->(void) THROWS((CNullPointerError))
        {
            return GetPointer();
        }

    // release
    inline
    TObjectType* ReleaseOrNull(void) THROWS_NONE
        {
            return const_cast<TObjectType*>(static_cast<const TObjectType*>(x_ReleaseOrNull()));
        }
    inline
    TObjectType* Release(void) THROWS((CNullPointerError))
        {
            return const_cast<TObjectType*>(static_cast<const TObjectType*>(x_Release()));
        }

    // assign
    CRef<C>& operator=(const CRef<C>& ref)
        {
            x_Reset(ref.GetPointerOrNull());
            return *this;
        }
};

template<class C>
class CConstRef : public CRefBase {
    typedef CRefBase CParent;
public:
    typedef const C TObjectType;

    // constructors
    inline
    CConstRef(void) THROWS_NONE
        {
        }
    CConstRef(TObjectType* ptr) THROWS_NONE
        : CParent(ptr)
        {
        }
    CConstRef(const CConstRef<C>& ref) THROWS_NONE
        : CParent(ref.GetPointerOrNull())
        {
        }
    CConstRef(const CRef<C>& ref) THROWS_NONE
        : CParent(ref.GetPointerOrNull())
        {
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

    // reset
    inline
    void Reset(TObjectType* ptr = 0)
        {
            x_Reset(ptr);
        }
    
    // getters
    inline
    TObjectType* GetPointerOrNull(void) const THROWS_NONE
        {
            return static_cast<TObjectType*>(x_GetPointerOrNull());
        }
    inline
    TObjectType* GetPointer(void) const THROWS((CNullPointerError))
        {
            return static_cast<TObjectType*>(x_GetPointer());
        }
    inline
    TObjectType& GetObject(void) const THROWS((CNullPointerError))
        {
            return *GetPointer();
        }
    inline
    TObjectType& operator*(void) const THROWS((CNullPointerError))
        {
            return GetObject();
        }
    inline
    TObjectType* operator->(void) const THROWS((CNullPointerError))
        {
            return GetPointer();
        }

    // release
    inline
    TObjectType* ReleaseOrNull(void) THROWS_NONE
        {
            return static_cast<TObjectType*>(x_ReleaseOrNull());
        }
    inline
    TObjectType* Release(void) THROWS((CNullPointerError))
        {
            return static_cast<TObjectType*>(x_Release());
        }

    // assign
    CConstRef<C>& operator=(const CConstRef<C>& ref)
        {
            Reset(ref.GetPointerOrNull());
            return *this;
        }
    CConstRef<C>& operator=(const CRef<C>& ref)
        {
            Reset(ref.GetPointerOrNull());
            return *this;
        }
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
