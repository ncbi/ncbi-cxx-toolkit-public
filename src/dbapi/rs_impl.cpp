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
* Revision 1.3  2002/04/05 19:29:50  kholodov
* Modified: GetBlobIStream() returns one and the same object, which is created
* on the first call.
* Added: GetVariant(const string& colName) to retrieve column value by
* column name
*
* Revision 1.2  2002/03/13 16:40:16  kholodov
* Modified indent formatting
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
* Revision 1.1  2001/11/30 16:30:06  kholodov
* Snapshot of the first draft of dbapi lib
*
*
*
*/

#include "stmt_impl.hpp"
#include "conn_impl.hpp"
#include "cursor_impl.hpp"
#include "rs_impl.hpp"
#include "rsmeta_impl.hpp"

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/exception.hpp>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

CResultSet::CResultSet(CConnection* conn, CDB_Result *rs)
    : m_conn(conn),
      m_rs(rs), m_istr(0), m_ostr(0)
{
    if( m_rs == 0 ) {
        SetValid(false);
    }
    else {
        // Reserve storage for column data
        EDB_Type type;
        for(unsigned int i = 0; i < m_rs->NofItems(); ++i ) {
            type = m_rs->ItemDataType(i);
            m_data.Add(CVariant(type));
        }
    }
}

CResultSet::~CResultSet()
{
    Close();
    Notify(CDbapiDeletedEvent(this));
    delete m_istr;
    delete m_ostr;
}

IResultSetMetaData* CResultSet::GetMetaData() 
{
    CheckValid();
    CResultSetMetaData *md = new CResultSetMetaData(m_rs);
    md->AddListener(this);
    AddListener(md);
    return md;
}

EDB_ResType CResultSet::GetResultType() 
{
    CheckValid();
    return m_rs->ResultType();
}

bool CResultSet::Next() 
{
    CheckValid();

    bool more = false;
    EDB_Type type = eDB_UnsupportedType;

    if( m_rs->Fetch() ) {
        for(unsigned int i = 0; i < m_rs->NofItems(); ++i ) {
      
            type = m_rs->ItemDataType(i);

            if( type == eDB_Text || type == eDB_Image ) {
                break;
            }

            m_rs->GetItem(m_data[i].GetNonNullData());
        }

        more = true;
    }
  
    return more;
}

size_t CResultSet::Read(void* buf, size_t size)
{
    return m_rs->ReadItem(buf, size);
}

istream& CResultSet::GetBlobIStream(size_t buf_size)
{
    if( m_istr == 0 ) {
        m_istr = new CBlobIStream(m_rs, buf_size);
    }
    return *m_istr;
}

ostream& CResultSet::GetBlobOStream(size_t blob_size, 
                                    size_t buf_size)
{
    // GetConnAux() returns pointer to pooled CDB_Connection.
    // we need to delete it every time we request new one.
    // The same with ITDescriptor
    delete m_ostr;

    // Call ReadItem(0, 0) before getting text/image descriptor
    m_rs->ReadItem(0, 0);

    m_ostr = new CBlobOStream(m_conn->GetConnAux(),
                              m_rs->GetImageOrTextDescriptor(),
                              blob_size,
                              buf_size);
    return *m_ostr;
}

void CResultSet::Close()
{
    delete m_rs;
    m_rs = 0;
    SetValid(false);
}
  
void CResultSet::Action(const CDbapiEvent& e) 
{
    if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {

        RemoveListener(dynamic_cast<IEventListener*>(e.GetSource()));

        if(dynamic_cast<CStatement*>(e.GetSource()) != 0
           || dynamic_cast<CCursor*>(e.GetSource()) != 0 ) {
            delete this;
            //SetValid(false);
        }
    }
}


int CResultSet::GetColNum(const string& name) {
    
    int i = 0;
    for( ; i < m_rs->NofItems(); ++i ) {
        
        if( !NStr::Compare(m_rs->ItemName(i), name) )
            return i;
    }

    throw CDbapiException("CResultSet::GetVariant(): invalid column name");
}

END_NCBI_SCOPE
