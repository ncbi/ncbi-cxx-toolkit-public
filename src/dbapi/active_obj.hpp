#ifndef _ACTIVE_OBJ_HPP_
#define _ACTIVE_OBJ_HPP_

/* $Id$
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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  CActiveObject and IEventListener, object notification
*                    interface
*
*
* $Log$
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <corelib/ncbiobj.hpp>
#include "invalidobjex.hpp"
#include <set>

BEGIN_NCBI_SCOPE

class CActiveObject;

class CDbapiEvent
{
public:

  CDbapiEvent(CActiveObject* src)
    : m_source(src) {}
  
  virtual ~CDbapiEvent() {}

  CActiveObject* GetSource() const { return m_source; }


private:
  CActiveObject* m_source;
};


class CDbapiDeletedEvent : public CDbapiEvent
{
public:
  CDbapiDeletedEvent(CActiveObject* src)
    : CDbapiEvent(src) {}

  virtual ~CDbapiDeletedEvent() {}
};

//===============================================================

class IEventListener
{
public:
  virtual void Action(const CDbapiEvent& ev) = 0;

protected:
  IEventListener() {}
};

//=================================================================
class CActiveObject : public CObject 
{
public:
  CActiveObject();
  virtual ~CActiveObject();

  void AddListener(IEventListener* obj);
  void RemoveListener(IEventListener* obj);
  void Notify(const CDbapiEvent& e);

  void CheckValid() const;
  void SetValid(bool v);


private:
  typedef set<IEventListener*> TLList;

  TLList m_listenerList;
  bool m_valid;

};


END_NCBI_SCOPE

#endif // _GENERAL_EXCEPTION_HPP_
