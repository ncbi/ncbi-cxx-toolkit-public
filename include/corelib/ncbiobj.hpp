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
        eDoNotDelete = 0x80000000u
    };

public:
    // main constructor for static/automatic/enclosed objects
    CObject(void)
        : m_Counter(eDoNotDelete)
        {
        }
    // virtual destructor
    virtual ~CObject(void) THROWS((runtime_error));

protected:
    // special constructor for objects allocated in heap
    enum ECanDelete {
        eCanDelete = 0
    };

    CObject(ECanDelete)
        : m_Counter(0)
        {
        }

public:
    inline
    unsigned ReferenceCount(void) const
        {
            return (m_Counter & ~eDoNotDelete);
        }

    inline
    bool Referenced(void) const
        {
            return ReferenceCount() != 0;
        }

    inline
    void AddReference(void)
        {
            ++m_Counter;
        }

    inline
    void RemoveReference(void) THROWS((runtime_error))
        {
            if ( unsigned(m_Counter-- & ~eDoNotDelete) <= 1 ) {
                RemoveLastReference();
            }
        }

    // remove reference without deleting object
    void ReleaseReference(void) THROWS((runtime_error));

private:
    void RemoveLastReference(void) THROWS((runtime_error));

    unsigned m_Counter;
};

template<class C>
class CRef {
public:
    typedef C TObjectType;

    inline
    CRef(void)
        : m_Ptr(0)
        {
        }
    CRef(TObjectType* ptr)
        : m_Ptr(ptr)
        {
            if ( ptr )
                ptr->AddReference();
        }
    CRef(const CRef<C>& ref)
        {
            TObjectType* ptr = m_Ptr = ref.m_Ptr;
            if ( ptr )
                ptr->AddReference();
        }
    ~CRef(void) THROWS((runtime_error))
        {
            TObjectType* ptr = m_Ptr;
            if ( ptr )
                ptr->RemoveReference();
        }
    
    // check whether reference in not null
    inline
    operator bool(void)
        {
            return m_Ptr != 0;
        }
    inline
    operator bool(void) const
        {
            return m_Ptr != 0;
        }

    inline
    void Reset(TObjectType* newPtr = 0) THROWS((runtime_error))
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

    // get object reference
    inline
    const TObjectType& operator*(void) const THROWS((CNullPointerError))
        {
            const TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return *ptr;
        }
    inline
    TObjectType& operator*(void) THROWS((CNullPointerError))
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return *ptr;
        }
    inline
    const TObjectType* operator->(void) const THROWS((CNullPointerError))
        {
            const TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return ptr;
        }
    inline
    TObjectType* operator->(void) THROWS((CNullPointerError))
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return ptr;
        }

    inline
    TObjectType* Release(void) THROWS((CNullPointerError))
        {
            TObjectType* ptr = m_Ptr;
            if ( !ptr )
                throw CNullPointerError();
            return ptr->ReleaseReference();
        }

private:
    TObjectType* m_Ptr;
};

END_NCBI_SCOPE

#endif
