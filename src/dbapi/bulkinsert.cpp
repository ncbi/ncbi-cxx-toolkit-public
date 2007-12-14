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
*/

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "bulkinsert.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_BulkInsert

BEGIN_NCBI_SCOPE

// implementation
CBulkInsert::CBulkInsert(const string& name,
                         unsigned int cols,
                         CConnection* conn)
    : m_nofCols(cols), m_cmd(0), m_conn(conn)
{
    m_cmd = m_conn->GetCDB_Connection()->BCPIn(name, cols);
    SetIdent("CBulkInsert");
}

CBulkInsert::~CBulkInsert()
{
    try {
        Notify(CDbapiClosedEvent(this));
        FreeResources();
        Notify(CDbapiDeletedEvent(this));
        _TRACE(GetIdent() << " " << (void*)this << " deleted."); 
    }
    NCBI_CATCH_ALL_X( 1, kEmptyStr )
}

void CBulkInsert::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}

void CBulkInsert::FreeResources()
{
    delete m_cmd;
    m_cmd = 0;
    if( m_conn != 0 && m_conn->IsAux() ) {
	    delete m_conn;
	    m_conn = 0;
	    Notify(CDbapiAuxDeletedEvent(this));
    }
}
 
void CBulkInsert::Bind(unsigned int col, CVariant* v)
{
    GetBCPInCmd()->BindParam(col - 1, v->GetData());
}
		
		
void CBulkInsert::AddRow()
{
    GetBCPInCmd()->SendRow();
}

void CBulkInsert::StoreBatch() 
{
    GetBCPInCmd()->CompleteBatch();
}

void CBulkInsert::Cancel()
{
    GetBCPInCmd()->Cancel();
}

void CBulkInsert::Complete()
{
    GetBCPInCmd()->CompleteBCP();
}

void CBulkInsert::Action(const CDbapiEvent& e) 
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
