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
* $Log$
* Revision 1.8  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.7  2004/02/27 14:37:32  kholodov
* Modified: set collection replaced by list for listeners
*
* Revision 1.6  2002/12/16 18:56:50  kholodov
* Fixed: memory leak in CStatement object
*
* Revision 1.5  2002/10/22 22:14:01  vakatov
* Get rid of a compilation warning
*
* Revision 1.4  2002/09/18 18:47:30  kholodov
* Modified: CActiveObject inherits directly from IEventListener
* Added: Additional trace output
*
* Revision 1.3  2002/09/09 20:48:56  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.2  2002/02/05 17:16:23  kholodov
* Put into right scope, invalidobjex retired
*
* Revision 1.1  2002/01/30 14:51:20  kholodov
* User DBAPI implementation, first commit
*
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include <typeinfo>
//#include "invalidobjex.hpp"

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
  m_listenerList.push_back(obj);
  _TRACE("Object " << obj->GetIdent() << " " << (void*)obj
         << " inserted into "
         << GetIdent() << " " << (void*)this << " listener list");
}

void CActiveObject::RemoveListener(CActiveObject* obj)
{
    m_listenerList.remove(obj);
    _TRACE("Object " << obj->GetIdent() << " " << (void*)obj
           << " removed from "
           << GetIdent() << " " << (void*)this << " listener list");
  
}

void CActiveObject::Notify(const CDbapiEvent& e)
{
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
//======================================================
