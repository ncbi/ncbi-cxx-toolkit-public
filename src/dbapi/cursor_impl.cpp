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
* File Description:  Cursor implementation class
*
*/

#include <ncbi_pch.hpp>
#include "conn_impl.hpp"
#include "cursor_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls

BEGIN_NCBI_SCOPE

// implementation
CCursor::CCursor(const string& name,
                 const string& sql,
                 int batchSize,
                 CConnection* conn)
    : m_cmd(0), m_conn(conn), m_ostr(0), m_wr(0)
{
    SetIdent("CCursor");

    m_cmd = m_conn->GetCDB_Connection()->Cursor(name, sql, batchSize);
}

CCursor::~CCursor()
{
    try {
        Notify(CDbapiClosedEvent(this));
        FreeResources();
        Notify(CDbapiDeletedEvent(this));
        _TRACE(GetIdent() << " " << (void*)this << " deleted."); 
    }
    NCBI_CATCH_ALL_X( 3, kEmptyStr )
}

  
IConnection* CCursor::GetParentConn() 
{
    return m_conn;
}

void CCursor::SetParam(const CVariant& v, 
                       const CDBParamVariant& param)
{
    if (param.IsPositional()) {
        // Decrement position by ONE.
        GetCursorCmd()->GetBindParams().Set(param.GetPosition() - 1, v.GetData());
    } else {
        GetCursorCmd()->GetBindParams().Set(param, v.GetData());
    }
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

CNcbiOstream& CCursor::GetBlobOStream(unsigned int col,
                                      size_t blob_size, 
                                      TBlobOStreamFlags flags,
                                      size_t buf_size)
{
    // Delete previous ostream
    delete m_ostr;
    m_ostr = 0;

    m_ostr = new CWStream(new CxBlobWriter(GetCursorCmd(),
                                           col - 1,
                                           blob_size,
                                           flags), 
                          buf_size, 0, (CRWStreambuf::fOwnWriter |
                                        CRWStreambuf::fLogExceptions));
    return *m_ostr;
}

IWriter* CCursor::GetBlobWriter(unsigned int col,
                                size_t blob_size, 
                                TBlobOStreamFlags flags)
{
    // Delete previous writer
    delete m_wr;
    m_wr = 0;

    m_wr = new CxBlobWriter(GetCursorCmd(),
                            col - 1,
                            blob_size,
                            flags);
    return m_wr;
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
