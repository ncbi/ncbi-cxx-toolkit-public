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
* Revision 1.2  2000/03/29 18:02:40  vasilche
* Workaroung of bug in MSVC: abstract member in template.
*
* Revision 1.1  2000/03/29 15:55:27  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE

template<class Iterator>
CTreeIteratorBase<Iterator>::~CTreeIteratorBase(void)
{
    Reset();
}

template<class Iterator>
void CTreeIteratorBase<Iterator>::Reset(void)
{
    m_CurrentObject.Reset();
    m_Valid = false;
    m_SkipSubTree = false;
    while ( m_Stack ) {
        m_Stack = m_Stack->Pop();
        --m_StackDepth;
    }
    _ASSERT(m_StackDepth == 0);
    _ASSERT(End());
}

template<class Iterator>
void CTreeIteratorBase<Iterator>::x_Begin(const TObjectInfo& info)
{
    Reset();
    if ( !info )
        return;
    m_CurrentObject = info;
    if ( CanSelect(info.GetTypeInfo()) ) {
        m_Valid = true;
    }
    else {
        m_Valid = true;
        Next();
    }
}

template<class Iterator>
void CTreeIteratorBase<Iterator>::Next(void)
{
    // cache object/type
    if ( End() )
        return;
    m_Valid = false;
    // traverse through tree
    for (;;) {
        if ( CanEnter(m_CurrentObject.GetTypeInfo()) ) {
            // go to child
            _TRACE("Children of "<<m_CurrentObject.GetTypeInfo()->GetName());
            m_Stack = new CTreeLevel(m_CurrentObject, m_Stack);
            ++m_StackDepth;
            // go to fetching object
        }
        else {
            m_SkipSubTree = false;
            // skip all finished iterators
            while ( m_Stack ) {
                m_Stack->Next();
                if ( *m_Stack ) {
                    // next child on this level
                    break;
                }
                _TRACE("End of children of "<<m_Stack->GetParent().GetTypeInfo()->GetName());
                // end of children on this level
                m_Stack = m_Stack->Pop();
                --m_StackDepth;
            }

            if ( !m_Stack ) {
                // end
                _TRACE("End of tree");
                _ASSERT(m_StackDepth == 0);
                m_CurrentObject.Reset();
                return;
            }
        }
        m_Stack->GetChild(m_CurrentObject);
        _TRACE("Next child: "<<m_CurrentObject.GetTypeInfo()->GetName());
        while ( m_CurrentObject &&
                m_CurrentObject.GetTypeInfo()->IsPointer() ) {
            m_CurrentObject.GetTypeInfo()->GetPointedObject(m_CurrentObject);
            _TRACE("Ptr: "<<m_CurrentObject.GetTypeInfo()->GetName());
        }
        if ( m_CurrentObject && CanSelect(m_CurrentObject.GetTypeInfo()) ) {
            m_Valid = true;
            _TRACE("Good!");
            return;
        }
    }
}

template<class Iterator>
bool CTreeIteratorBase<Iterator>::CanSelect(TTypeInfo /*info*/) const
{
	return false;
}

template<class Iterator>
bool CTreeIteratorBase<Iterator>::CanEnter(TTypeInfo /*info*/) const
{
    const TObjectInfo& obj = m_CurrentObject;
    return !m_SkipSubTree && obj &&
        obj.GetTypeInfo()->HaveChildren(obj.GetObjectPtr());
}

template class CTreeIteratorBase<CConstChildrenIterator>;
template class CTreeIteratorBase<CChildrenIterator>;

void CTreeIterator::Erase(void)
{
    if ( !Valid() )
        THROW1_TRACE(runtime_error, "cannot erase nonvalid iterator");
    if ( !m_Stack )
        THROW1_TRACE(runtime_error, "cannot erase root object of iterator");
    m_CurrentObject.Reset();
    m_Stack->Erase();
    m_Valid = false;
    m_SkipSubTree = true;
}

template<class Iterator>
bool CTypeIteratorBase<Iterator>::CanSelect(TTypeInfo info) const
{
    return info->IsType(m_NeedType);
}

template<class Iterator>
bool CTypeIteratorBase<Iterator>::CanEnter(TTypeInfo info) const
{
    return CParent::CanEnter(info) && info->MayContainType(m_NeedType);
}

template class CTypeIteratorBase<CTreeIterator>;
template class CTypeIteratorBase<CTreeConstIterator>;

END_NCBI_SCOPE
