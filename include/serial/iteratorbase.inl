#if defined(ITERATORBASE__HPP)  &&  !defined(ITERATORBASE__INL)
#define ITERATORBASE__INL

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
* Revision 1.1  2000/04/10 21:01:39  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

inline
CChildrenTypesIterator::CChildrenTypesIterator(TTypeInfo parentType)
    : m_Parent(parentType)
{
    if ( !parentType )
        THROW0_TRACE(CNullPointerError());
    parentType->BeginTypes(*this);
}

inline
CChildrenTypesIterator::operator bool(void) const
{
    return GetTypeInfo()->ValidTypes(*this);
}

inline
void CChildrenTypesIterator::Next(void)
{
    GetTypeInfo()->NextType(*this);
}

inline
TTypeInfo CChildrenTypesIterator::GetChildType(void) const
{
    return GetTypeInfo()->GetChildType(*this);
}

inline
CConstChildrenIterator::CConstChildrenIterator(const CConstObjectInfo& parent)
    : m_Parent(parent)
{
    if ( !parent )
        THROW0_TRACE(CNullPointerError());
    GetTypeInfo()->Begin(*this);
}

inline
CConstChildrenIterator::operator bool(void) const
{
    return GetTypeInfo()->Valid(*this);
}

inline
void CConstChildrenIterator::Next(void)
{
    GetTypeInfo()->Next(*this);
}

inline
void CConstChildrenIterator::GetChild(CConstObjectInfo& child) const
{
    GetTypeInfo()->GetChild(*this, child);
}

inline
CChildrenIterator::CChildrenIterator(const CObjectInfo& parent)
    : m_Parent(parent)
{
    if ( !parent )
        THROW0_TRACE(CNullPointerError());
    GetTypeInfo()->Begin(*this);
}

inline
CChildrenIterator::operator bool(void) const
{
    return GetTypeInfo()->Valid(*this);
}

inline
void CChildrenIterator::Next(void)
{
    GetTypeInfo()->Next(*this);
}

inline
void CChildrenIterator::GetChild(CObjectInfo& child) const
{
    GetTypeInfo()->GetChild(*this, child);
}

inline
void CChildrenIterator::Erase(void)
{
    GetTypeInfo()->Erase(*this);
}

END_NCBI_SCOPE

#endif /* def ITERATORBASE__HPP  &&  ndef ITERATORBASE__INL */
