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
* File Description:  Base class for database access
*
*
* $Log$
* Revision 1.16  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.15  2004/04/26 14:16:56  kholodov
* Modified: recreate the command objects each time the Get...() is called
*
* Revision 1.14  2004/04/22 14:22:25  kholodov
* Added: Cancel()
*
* Revision 1.13  2004/04/12 14:25:33  kholodov
* Modified: resultset caching scheme, fixed single connection handling
*
* Revision 1.12  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.11  2004/03/08 22:15:19  kholodov
* Added: 3 new Get...() methods internally
*
* Revision 1.10  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.9  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.8  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.7  2002/09/16 19:34:40  kholodov
* Added: bulk insert support
*
* Revision 1.6  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.5  2002/08/26 15:35:56  kholodov
* Added possibility to disable transaction log
* while updating BLOBs
*
* Revision 1.4  2002/07/08 16:06:37  kholodov
* Added GetBlobOStream() implementation
*
* Revision 1.3  2002/05/16 22:11:12  kholodov
* Improved: using minimum connections possible
*
* Revision 1.2  2002/02/08 17:38:26  kholodov
* Moved listener registration to parent objects
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
* Revision 1.1  2001/11/30 16:30:05  kholodov
* Snapshot of the first draft of dbapi lib
*
*
*
*/

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "cursor_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

// implementation
CCursor::CCursor(const string& name,
                 const string& sql,
                 int nofArgs,
                 int batchSize,
                 CConnection* conn)
    : m_nofArgs(nofArgs), m_cmd(0), m_conn(conn), m_ostr(0)
{
    SetIdent("CCursor");

    m_cmd = m_conn->GetCDB_Connection()->Cursor(name, sql,
                                                nofArgs, batchSize);
}

CCursor::~CCursor()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
    Notify(CDbapiDeletedEvent(this));
    _TRACE(GetIdent() << " " << (void*)this << " deleted."); 
}

  
IConnection* CCursor::GetParentConn() 
{
    return m_conn;
}

void CCursor::SetParam(const CVariant& v, 
                       const string& name)
{


    Parameters::iterator cur = m_params.find(name);
    if( cur != m_params.end() ) {
        *((*cur).second) = v;
    }
    else {
        pair<Parameters::iterator, bool> 
            ins = m_params.insert(make_pair(name, new CVariant(v)));

        cur = ins.first;
    }


    GetCursorCmd()->BindParam((*cur).first,
                              (*cur).second->GetData());
}
		
		
IResultSet* CCursor::Open()
{
    CResultSet *ri = new CResultSet(m_conn, GetCursorCmd()->Open());
    ri->AddListener(this);
    AddListener(ri);
    return ri;
}

void CCursor::Update(const string& table, const string& updateSql) 
{
    GetCursorCmd()->Update(table, updateSql);
}

void CCursor::Delete(const string& table)
{
    GetCursorCmd()->Delete(table);
}

ostream& CCursor::GetBlobOStream(unsigned int col,
                                 size_t blob_size, 
                                 EAllowLog log_it,
                                 size_t buf_size)
{
    // Delete previous ostream
    delete m_ostr;

    m_ostr = new CBlobOStream(GetCursorCmd(),
                              col - 1,
                              blob_size,
                              buf_size,
                              log_it == eEnableLog);
    return *m_ostr;
}

void CCursor::Cancel()
{
    if( GetCursorCmd() != 0 )
        GetCursorCmd()->Close();
}

void CCursor::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}

void CCursor::FreeResources() 
{

    Parameters::iterator i = m_params.begin();
    for( ; i != m_params.end(); ++i ) {
        delete (*i).second;
    }

    m_params.clear();
    delete m_cmd;
    m_cmd = 0;
    delete m_ostr;
    m_ostr = 0;
    if( m_conn != 0 && m_conn->IsAux() ) {
	    delete m_conn;
	    m_conn = 0;
	    Notify(CDbapiAuxDeletedEvent(this));
    }
  
}

void CCursor::Action(const CDbapiEvent& e) 
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName() 
           << "' from " << e.GetSource()->GetIdent());

    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
	RemoveListener(e.GetSource());
        if(dynamic_cast<CConnection*>(e.GetSource()) != 0 ) {
            _TRACE("Deleting " << GetIdent() << " " << (void*)this); 
            delete this;
        }
    }
}

END_NCBI_SCOPE
