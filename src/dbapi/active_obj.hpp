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
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CActiveObject;

class CDbapiEvent
{
public:
   
    CDbapiEvent(CActiveObject* src, const string& name)
        : m_source(src), m_name(name) {}
  
    virtual ~CDbapiEvent() {}

    CActiveObject* GetSource() const { return m_source; }

    string GetName() const { return m_name; }
    
private:
    CActiveObject* m_source;
    string m_name;
};


class CDbapiDeletedEvent : public CDbapiEvent
{
public:
    CDbapiDeletedEvent(CActiveObject* src)
        : CDbapiEvent(src, "CDbapiDeletedEvent") {}

    virtual ~CDbapiDeletedEvent() {}
};

class CDbapiAuxDeletedEvent : public CDbapiEvent
{
public:
    CDbapiAuxDeletedEvent(CActiveObject* src)
        : CDbapiEvent(src, "CDbapiAuxDeletedEvent") {}

    virtual ~CDbapiAuxDeletedEvent() {}
};

class CDbapiNewResultEvent : public CDbapiEvent
{
public:
    CDbapiNewResultEvent(CActiveObject* src)
        : CDbapiEvent(src, "CDbapiNewResultEvent") {}

    virtual ~CDbapiNewResultEvent() {}
};

class CDbapiFetchCompletedEvent : public CDbapiEvent
{
public:
    CDbapiFetchCompletedEvent(CActiveObject* src)
        : CDbapiEvent(src, "CDbapiFetchCompletedEvent") {}

    virtual ~CDbapiFetchCompletedEvent() {}
};

class CDbapiClosedEvent : public CDbapiEvent
{
public:
    CDbapiClosedEvent(CActiveObject* src)
        : CDbapiEvent(src, "CDbapiClosedEvent") {}

    virtual ~CDbapiClosedEvent() {}
};

//===============================================================

class IEventListener
{
public:
    virtual ~IEventListener() {}
    virtual void Action(const CDbapiEvent& e) = 0;

protected:
    IEventListener() {}
};

//=================================================================
class CActiveObject : //public CObject,
                      public IEventListener
{
public:
    CActiveObject();
    virtual ~CActiveObject();

    void AddListener(CActiveObject* obj);
    void RemoveListener(CActiveObject* obj);
    void Notify(const CDbapiEvent& e);

    // Do nothing by default
    virtual void Action(const CDbapiEvent& e);

    string GetIdent() const;

protected:
    typedef list<CActiveObject*> TLList;

    void SetIdent(const string& name);

    TLList& GetListenerList();

private:

    TLList m_listenerList;
    string m_ident;  // Object identificator
    CMutex m_listMutex;
};


END_NCBI_SCOPE

#endif // _ACTIVE_OBJ_HPP_
