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

#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include <typeinfo>
//#include "invalidobjex.hpp"

BEGIN_NCBI_SCOPE

CActiveObject::CActiveObject() : m_valid(true)
{
}

CActiveObject::~CActiveObject() 
{
}

void CActiveObject::AddListener(IEventListener* obj)
{
  m_listenerList.insert(obj);
}

void CActiveObject::RemoveListener(IEventListener* obj)
{
  TLList::iterator i = m_listenerList.find(obj);
  if( i != m_listenerList.end() ) {
    m_listenerList.erase(i);
  }
}

void CActiveObject::Notify(const CDbapiEvent& e)
{
  TLList::iterator i = m_listenerList.begin();
  for( ; i != m_listenerList.end(); ++i ) {
    (*i)->Action(e);
  }
}
  
void CActiveObject::CheckValid() const
{
  // Disable check, not used anymore
  //    if( !m_valid ) 
  //throw CInvalidObjEx("CActiveObject::CheckValid(): invalid object state");
}

void CActiveObject::SetValid(bool v)
{
  m_valid = v;
}

END_NCBI_SCOPE
//======================================================
