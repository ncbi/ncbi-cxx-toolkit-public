#if defined(NCBIOBJ__HPP)  &&  !defined(NCBIOBJ__INL)
#define NCBIOBJ__INL

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
 *   Standard CObject and CRef classes for reference counter based GC.
 *   Inline functions.
 *
 */

#ifndef _DEBUG
inline
void CObject::operator delete(void* ptr)
{
    ::operator delete(ptr);
}

inline
void CObject::operator delete[](void* ptr)
{
# ifdef HAVE_WINDOWS_H
    ::operator delete(ptr);
# else
    ::operator delete[](ptr);
# endif
}
#endif  /* _DEBUG */


inline
bool CObject::ObjectStateCanBeDeleted(TCounter counter)
{
    return (counter & eStateBitsInHeap) != 0;
}


inline
bool CObject::ObjectStateValid(TCounter counter)
{
    return counter >= TCounter(eCounterValid);
}


inline
bool CObject::ObjectStateReferenced(TCounter counter)
{
    return counter >= TCounter(eCounterValid + eCounterStep);
}


inline
bool CObject::ObjectStateDoubleReferenced(TCounter counter)
{
    return counter >= TCounter(eCounterValid + eCounterStep * 2);
}


inline
bool CObject::ObjectStateReferencedOnlyOnce(TCounter counter)
{
    return ObjectStateReferenced(counter) &&
        !ObjectStateDoubleReferenced(counter);
}


inline
bool CObject::CanBeDeleted(void) const THROWS_NONE
{
    return ObjectStateCanBeDeleted(m_Counter);
}


inline
bool CObject::Referenced(void) const THROWS_NONE
{
    return ObjectStateReferenced(m_Counter);
}


inline
bool CObject::ReferencedOnlyOnce(void) const THROWS_NONE
{
    return ObjectStateReferencedOnlyOnce(m_Counter);
}


inline
CObject& CObject::operator=(const CObject& ) THROWS_NONE
{
    return *this;
}


inline
void CObject::AddReference(void) const
{
    TCounter oldCounter;

    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        oldCounter = m_Counter;

        TCounter newCounter = oldCounter + TCounter(eCounterStep);
        if ( ObjectStateReferenced(newCounter) ) {
            m_Counter = newCounter;
            return;
        }
    }}
    
    AddReferenceOverflow(oldCounter);
}


inline
void CObject::RemoveReference(void) const
{
    {{
        CFastMutexGuard LOCK(sm_ObjectMutex);
        if ( ObjectStateDoubleReferenced(m_Counter) ) {
            m_Counter -= eCounterStep;
            return;
        }
    }}

    RemoveLastReference();
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.5  2001/03/26 21:22:51  vakatov
 * Minor cosmetics
 *
 * Revision 1.4  2001/03/13 22:43:49  vakatov
 * Made "CObject" MT-safe
 * + CObject::DoDeleteThisObject()
 *
 * Revision 1.3  2000/11/01 21:20:34  vasilche
 * Fixed missing new[] and delete[] on MSVC.
 *
 * Revision 1.2  2000/11/01 20:35:02  vasilche
 * Fixed detection of heap objects.
 * Removed ECanDelete enum and related constructors.
 *
 * Revision 1.1  2000/10/13 16:39:16  vasilche
 * Forgot to add file.
 *
 * ===========================================================================
 */

#endif /* def NCBIOBJ__HPP  &&  ndef NCBIOBJ__INL */
