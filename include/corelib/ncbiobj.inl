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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/10/13 16:39:16  vasilche
* Forgot to add file.
*
* ===========================================================================
*/

#ifndef _DEBUG
// inline initialization without additional check only in release mode
inline
void CObject::InitInStack(void)
{
    m_Counter = eObjectInStackUnsure;
}

inline
void CObject::InitInHeap(void)
{
    m_Counter = eObjectInHeapUnsure;
}
#endif

inline
void CObject::SetCanDelete(void)
{
    if ( m_Counter == TCounter(eObjectInStackUnsure) )
        m_Counter = eObjectInHeap;
    else
        SetCanDeleteLong();
}

inline
CObject::CObject(void)
{
    InitInStack();
}

inline
CObject::CObject(ECanDelete)
{
    InitInHeap();
}

inline
CObject::CObject(const CObject& /*src*/)
{
    InitInStack();
}

inline
CObject::CObject(ECanDelete, const CObject& /*src*/)
{
    InitInHeap();
}

inline
bool CObject::ObjectStateIsValid(TCounter counter)
{
    return counter >= TCounter(eMinimumValidCounter);
}

inline
bool CObject::ObjectStateIsInvalid(TCounter counter)
{
    return counter < TCounter(eMinimumValidCounter);
}

inline
bool CObject::ObjectStateCanBeDeleted(TCounter counter)
{
    return (counter & eStateBitsInHeap) != 0;
}

inline
bool CObject::ObjectStateUnsure(TCounter counter)
{
    return (counter & eStateBitsUnsure) != 0;
}

inline
bool CObject::ObjectStateReferenced(TCounter counter)
{
    return counter >= TCounter(eMinimumValidCounter + eCounterStep);
}

inline
bool CObject::ObjectStateDoubleReferenced(TCounter counter)
{
    return counter >= TCounter(eMinimumValidCounter + eCounterStep * 2);
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
CObject& CObject::operator=(const CObject& src) THROWS_NONE
{
    if ( ObjectStateIsInvalid(m_Counter) ||
         ObjectStateIsInvalid(src.m_Counter) )
        InvalidObject();
    return *this;
}

inline
void CObject::AddReference(void) const
{
    TCounter newCounter = m_Counter + TCounter(eCounterStep);
    if ( ObjectStateReferenced(newCounter) )
        m_Counter = newCounter;
    else
        AddReferenceOverflow();
}

inline
void CObject::RemoveReference(void) const
{
    TCounter oldCounter = m_Counter;
    if ( ObjectStateDoubleReferenced(oldCounter) )
        m_Counter = oldCounter - eCounterStep;
    else
        RemoveLastReference();
}

#endif /* def NCBIOBJ__HPP  &&  ndef NCBIOBJ__INL */
