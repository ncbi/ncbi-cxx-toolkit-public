#if defined(CORELIB___NCBIOBJ__HPP)  &&  !defined(CORELIB___NCBIOBJ__INL)
#define CORELIB___NCBIOBJ__INL

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


inline
bool CObject::ObjectStateCanBeDeleted(TCount count)
{
    return (count & eStateBitsInHeap) != 0;
}


inline
bool CObject::ObjectStateIsAllocatedInPool(TCount count)
{
    return (count & eStateBitsInHeapMask) ==
        (eInitCounterInPool & eStateBitsInHeapMask);
}


inline
bool CObject::ObjectStateValid(TCount count)
{
    return count >= TCount(eCounterValid);
}


inline
bool CObject::ObjectStateReferenced(TCount count)
{
    return count >= TCount(eCounterValidRef1);
}


inline
bool CObject::ObjectStateUnreferenced(TCount count)
{
    return (count & ~eStateBitsInHeapMask) == eCounterValid;
}


inline
bool CObject::ObjectStateReferencedOnlyOnce(TCount count)
{
    return (count & ~eStateBitsInHeapMask) == eCounterValidRef1;
}


inline
bool CObject::CanBeDeleted(void) const THROWS_NONE
{
    return ObjectStateCanBeDeleted(m_Counter.Get());
}


inline
bool CObject::IsAllocatedInPool(void) const THROWS_NONE
{
    return ObjectStateIsAllocatedInPool(m_Counter.Get());
}


inline
bool CObject::Referenced(void) const THROWS_NONE
{
    return ObjectStateReferenced(m_Counter.Get());
}


inline
bool CObject::ReferencedOnlyOnce(void) const THROWS_NONE
{
    return ObjectStateReferencedOnlyOnce(m_Counter.Get());
}


inline
CObject& CObject::operator=(const CObject& ) THROWS_NONE
{
    return *this;
}


inline
void CObject::AddReference(void) const
{
    TCount newCount = m_Counter.Add(eCounterStep);
    if ( !ObjectStateReferenced(newCount) ) {
        m_Counter.Add(-eCounterStep); // undo
        CheckReferenceOverflow(newCount - eCounterStep);
    }
}


inline
void CObject::RemoveReference(void) const
{
    TCount newCount = m_Counter.Add(-eCounterStep);
    if ( !ObjectStateReferenced(newCount) ) {
        RemoveLastReference();
    }
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2005/04/26 14:08:33  vasilche
 * Allow allocation of CObjects from CObjectMemoryPool.
 * Documented CObject counter bits.
 *
 * Revision 1.11  2005/03/17 19:54:30  grichenk
 * DoDeleteThisObject() fails for objects not in heap.
 *
 * Revision 1.10  2003/10/20 20:35:31  ucko
 * Make include guards consistent with ncbiobj.hpp's again, so the build
 * doesn't totally break.
 *
 * Revision 1.9  2003/09/17 15:20:45  vasilche
 * Moved atomic counter swap functions to separate file.
 * Added CRef<>::AtomicResetFrom(), CRef<>::AtomicReleaseTo() methods.
 *
 * Revision 1.8  2003/08/12 12:04:49  siyan
 * Changed reference to AddReferenceOverflow() to its new name of
 * CheckReferenceOverflow().
 *
 * Revision 1.7  2002/05/23 22:24:21  ucko
 * Use low-level atomic operations for reference counts
 *
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

#endif /* def CORELIB___NCBIOBJ__HPP  &&  ndef CORELIB___NCBIOBJ__INL */
