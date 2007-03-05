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
* Author: Michael Kholodov
*
* File Description:  CActiveObject and IEventListener, object notification
*                    interface
*
* ---------------------------------------------------------------------------
*/

#include <ncbi_pch.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include <typeinfo>

BEGIN_NCBI_SCOPE

CActiveObject::CActiveObject() 
{
    SetIdent("ActiveObject");
}

CActiveObject::~CActiveObject() 
{
}

void CActiveObject::AddListener(CActiveObject* obj)
{
    CMutexGuard guard(m_listMutex);

    m_listenerList.push_back(obj);
    _TRACE("Object " << obj->GetIdent() << " " << (void*)obj
         << " inserted into "
         << GetIdent() << " " << (void*)this << " listener list");
}

void CActiveObject::RemoveListener(CActiveObject* obj)
{
    CMutexGuard guard(m_listMutex);

    m_listenerList.remove(obj);
    _TRACE("Object " << obj->GetIdent() << " " << (void*)obj
           << " removed from "
           << GetIdent() << " " << (void*)this << " listener list");
  
}

void CActiveObject::Notify(const CDbapiEvent& e)
{
    CMutexGuard guard(m_listMutex);

    TLList::iterator i = m_listenerList.begin();
    for( ; i != m_listenerList.end(); ++i ) {
        _TRACE("Object " << GetIdent() << " " << (void*)this
             << " notifies " << (*i)->GetIdent() << " " << (void*)(*i));
        (*i)->Action(e);
    }
}

void CActiveObject::Action(const CDbapiEvent&)
{

}

CActiveObject::TLList& CActiveObject::GetListenerList()
{
    return m_listenerList;
}
  
string CActiveObject::GetIdent() const
{
    return m_ident;
}

void CActiveObject::SetIdent(const string& name)
{
  m_ident = name;
}

END_NCBI_SCOPE
