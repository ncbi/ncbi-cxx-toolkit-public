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
* File Description:  DataSource implementation
*
*
* $Log$
* Revision 1.8  2002/09/30 19:16:27  kholodov
* Added: public GetHandler() method
*
* Revision 1.7  2002/09/23 18:35:24  kholodov
* Added: GetErrorInfo() and GetErrorAsEx() methods.
*
* Revision 1.6  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.5  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.4  2002/04/15 19:11:42  kholodov
* Changed GetContext() -> GetDriverContext
*
* Revision 1.3  2002/02/08 17:38:26  kholodov
* Moved listener registration to parent objects
*
* Revision 1.2  2002/02/06 22:21:57  kholodov
* Reformatted the source code
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include "ds_impl.hpp"
#include "conn_impl.hpp"
#include "dbexception.hpp"


CDataSource::CDataSource(I_DriverContext *ctx)
    : m_loginTimeout(30), m_context(ctx), m_poolUsed(false),
      m_multiExH(0)
{
    SetIdent("CDataSource");
}

CDataSource::~CDataSource()
{
    Notify(CDbapiDeletedEvent(this));
    delete m_context;
    delete m_multiExH;
}

void CDataSource::SetLoginTimeout(unsigned int i) 
{
    m_loginTimeout = i;
    if( m_context != 0 ) {
        m_context->SetLoginTimeout(i);
    }
}

void CDataSource::SetLogStream(CNcbiOstream* out) 
{
    if( out != 0 ) {
        // Clear the previous handlers if present
        if( m_multiExH != 0 ) {
            m_context->PopCntxMsgHandler(m_multiExH);
            m_context->PopDefConnMsgHandler(m_multiExH);
            delete m_multiExH;
            m_multiExH = 0;
        }
            
        CDB_UserHandler *newH = new CDB_UserHandler_Stream(out);
        CDB_UserHandler *h = CDB_UserHandler::SetDefault(newH);
        delete h;
    }
    else {
        if( m_multiExH == 0 ) {
            m_multiExH = new CToMultiExHandler;

            m_context->PushCntxMsgHandler(m_multiExH);
            m_context->PushDefConnMsgHandler(m_multiExH);
        }
    }
}

CToMultiExHandler* CDataSource::GetHandler()
{
    return m_multiExH;
}
    
CDB_MultiEx* CDataSource::GetErrorAsEx()
{
    return GetHandler() == 0 ? 0 : GetHandler()->GetMultiEx();
}

string CDataSource::GetErrorInfo()
{
    if( GetHandler() != 0 ) {
        CNcbiOstrstream out;
        CDB_UserHandler_Stream h(&out);
        h.HandleIt(GetHandler()->GetMultiEx());
        return CNcbiOstrstreamToString(out);
    }
    else
        return kEmptyStr;
}



I_DriverContext* CDataSource::GetDriverContext() {
    if( m_context == 0 )
      throw CDbapiException("CDataSource::GetDriverContext(): no valid context");

    return m_context;
  }

IConnection* CDataSource::CreateConnection()
{
    CConnection *conn = new CConnection(this);
    AddListener(conn);
    conn->AddListener(this);
    return conn;
}

void CDataSource::Action(const CDbapiEvent& e) 
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName() 
           << "' from " << e.GetSource()->GetIdent());

    if( dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
        RemoveListener(e.GetSource());
    }
}

//====================================================================
CToMultiExHandler::CToMultiExHandler()
    : m_ex(0)
{
    m_ex = new CDB_MultiEx("DBAPI message collector");
}

CToMultiExHandler::~CToMultiExHandler()
{
    delete m_ex;
}

bool CToMultiExHandler::HandleIt(CDB_Exception* ex)
{
    m_ex->Push(*ex);
       
    return true;
}
