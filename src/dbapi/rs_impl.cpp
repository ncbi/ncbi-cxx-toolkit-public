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
* File Description:  Resultset implementation
*
*/

#include <ncbi_pch.hpp>
#include "stmt_impl.hpp"
#include "cstmt_impl.hpp"
#include "conn_impl.hpp"
#include "cursor_impl.hpp"
#include "rs_impl.hpp"
#include "rsmeta_impl.hpp"
#include "dbexception.hpp"

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/exception.hpp>

#include <corelib/ncbistr.hpp>

#include <typeinfo>


BEGIN_NCBI_SCOPE

CResultSet::CResultSet(CConnection* conn, CDB_Result *rs)
    : m_conn(conn),
      m_rs(rs), m_istr(0), m_ostr(0), m_column(-1),
      m_bindBlob(false), m_disableBind(false), m_wasNull(true),
      m_rd(0), m_totalRows(0)
{
    SetIdent("CResultSet");

    if( m_rs == 0 ) {
        _TRACE("CResultSet::ctor(): null CDB_Result* object");
        _ASSERT(0);
    }
    else {
        Init();
    }
}


void CResultSet::Init()
{
    // Reserve storage for column data
    EDB_Type type;
    for(unsigned int i = 0; i < m_rs->NofItems(); ++i ) {
        type = m_rs->ItemDataType(i);
        switch( type ) {
        case eDB_Char:
            m_data.push_back(CVariant::Char(m_rs->ItemMaxSize(i), 0));
            break;
        case eDB_Binary:
            m_data.push_back(CVariant::Binary(m_rs->ItemMaxSize(i), 0, 0));
            break;
        case eDB_LongChar:
            m_data.push_back(CVariant::LongChar(0, m_rs->ItemMaxSize(i)));
            break;
        case eDB_LongBinary:
            m_data.push_back(CVariant::LongBinary(m_rs->ItemMaxSize(i), 0, 0));
            break;
        default:
            m_data.push_back(CVariant(type));
            break;
        }
    }

    _TRACE("CResultSet::Init(): Space reserved for " << m_data.size()
           << " columns");

}

CResultSet::~CResultSet()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
    Notify(CDbapiDeletedEvent(this));
    _TRACE(GetIdent() << " " << (void*)this << " deleted.");
}

const CVariant& CResultSet::GetVariant(unsigned int idx)
{
    CheckIdx(idx);
    return m_data[idx-1];
}

const CVariant& CResultSet::GetVariant(const string& name)
{
    int zIdx = GetColNum(name);

    CheckIdx(zIdx);

    return m_data[zIdx-1];
}


const IResultSetMetaData* CResultSet::GetMetaData()
{
    CResultSetMetaData *md = new CResultSetMetaData(m_rs);
    md->AddListener(this);
    AddListener(md);
    return md;
}

EDB_ResType CResultSet::GetResultType()
{
    return m_rs->ResultType();
}

void CResultSet::BindBlobToVariant(bool b)
{
    m_bindBlob = b;
}

void CResultSet::DisableBind(bool b)
{
    m_disableBind = b;
}

bool CResultSet::Next()
{

    bool more = false;
    EDB_Type type = eDB_UnsupportedType;

    more = m_rs->Fetch();


    if( more && !IsDisableBind() ) {

        for(unsigned int i = 0; i < m_rs->NofItems(); ++i ) {

            type = m_rs->ItemDataType(i);

            if( !IsBindBlob() ) {
                if( type == eDB_Text || type == eDB_Image )  {
                    m_column = m_rs->CurrentItemNo();
                    break;
                }
            }
            else {
                switch(type) {
                case eDB_Text:
                    ((CDB_Text*)m_data[i].GetNonNullData())->Truncate();
                    break;
                case eDB_Image:
                    ((CDB_Image*)m_data[i].GetNonNullData())->Truncate();
                    break;
                default:
                    break;
                }
            }

            m_rs->GetItem(m_data[i].GetNonNullData());
        }

    }

    if( !more ) {
        if( m_ostr ) {
            _TRACE("CResulstSet: deleting BLOB output stream...");
                   delete m_ostr;
                   m_ostr = 0;
        }
        if( m_istr ) {
            _TRACE("CResulstSet: deleting BLOB input stream...");
            delete m_istr;
            m_istr = 0;
        }
        if( m_rd ) {
            _TRACE("CResulstSet: deleting BLOB reader...");
            delete m_rd;
            m_rd = 0;
        }

        Notify(CDbapiFetchCompletedEvent(this));
    }
    else {
        ++m_totalRows;
    }

    return more;
}

size_t CResultSet::Read(void* buf, size_t size)
{

    if( m_column < 0 ) {
        _TRACE("CResulstSet: Column for raw Read not set, current column: "
                << m_rs->CurrentItemNo());
#ifdef _DEBUG
        _ASSERT(0);
#else
        NCBI_DBAPI_THROW( "Column for BLOB I/O is not initialized" );
#endif
    }
    else {
        _TRACE("Last column: " << m_column);
    }

    if( m_column != m_rs->CurrentItemNo() ) {

        m_column = m_rs->CurrentItemNo();
        return 0;
    }
    else {
        int ret = m_rs->ReadItem(buf, size, &m_wasNull);
        if( ret == 0 ) {
            m_column = m_rs->CurrentItemNo();
        }
        return ret;
    }
}

bool CResultSet::WasNull()
{
    return m_wasNull;
}


int CResultSet::GetColumnNo()
{
    int col = m_rs->CurrentItemNo();

    return col >= 0 ? col + 1 : -1;
}

unsigned int CResultSet::GetTotalColumns()
{
    return m_rs->NofItems();
}

istream& CResultSet::GetBlobIStream(size_t buf_size)
{
#if 0
    if( m_istr == 0 ) {
        m_istr = new CBlobIStream(this, buf_size);
    }
#endif
    delete m_istr;
    m_istr = new CBlobIStream(this, buf_size);

    return *m_istr;
}

IReader* CResultSet::GetBlobReader()
{
    delete m_rd;
    m_rd = new CBlobReader(this);

    return m_rd;
}

ostream& CResultSet::GetBlobOStream(size_t blob_size,
                                    EAllowLog log_it,
                                    size_t buf_size)
{
    // GetConnAux() returns pointer to pooled CDB_Connection.
    // we need to delete it every time we request new one.
    // The same with ITDescriptor
    delete m_ostr;

    // Call ReadItem(0, 0) before getting text/image descriptor
    m_rs->ReadItem(0, 0);


    I_ITDescriptor* desc = m_rs->GetImageOrTextDescriptor();
    if( desc == 0 ) {
#ifdef _DEBUG
        NcbiCerr << "CResultSet::GetBlobOStream(): zero IT Descriptor" << endl;
        _ASSERT(0);
#else
        NCBI_DBAPI_THROW( "CResultSet::GetBlobOStream(): Invalid IT Descriptor" );
#endif
    }

    m_ostr = new CBlobOStream(m_conn->CloneCDB_Conn(),
                              desc,
                              blob_size,
                              buf_size,
                              log_it == eEnableLog,
                              true);
    return *m_ostr;
}

ostream& CResultSet::GetBlobOStream(IConnection *conn, size_t blob_size,
                                    EAllowLog log_it,
                                    size_t buf_size)
{
    // GetConnAux() returns pointer to pooled CDB_Connection.
    // we need to delete it every time we request new one.
    // The same with ITDescriptor
    delete m_ostr;

    // Call ReadItem(0, 0) before getting text/image descriptor
    m_rs->ReadItem(0, 0);


    I_ITDescriptor* desc = m_rs->GetImageOrTextDescriptor();
    if( desc == 0 ) {
#ifdef _DEBUG
        NcbiCerr << "CResultSet::GetBlobOStream(): zero IT Descriptor" << endl;
        _ASSERT(0);
#else
        NCBI_DBAPI_THROW( "CResultSet::GetBlobOStream(): Invalid IT Descriptor" );
#endif
    }

    m_ostr = new CBlobOStream(conn->GetCDB_Connection(),
                              desc,
                              blob_size,
                              buf_size,
                              log_it == eEnableLog);
    return *m_ostr;
}

void CResultSet::Close()
{
    Notify(CDbapiClosedEvent(this));
    FreeResources();
}

void CResultSet::FreeResources()
{
    //_TRACE("CResultSet::Close(): deleting CDB_Result " << (void*)m_rs);
    Invalidate();

    delete m_istr;
    m_istr = 0;
    delete m_ostr;
    m_ostr = 0;
    delete m_rd;
    m_rd = 0;
    m_totalRows = -1;
}

void CResultSet::Action(const CDbapiEvent& e)
{
    _TRACE(GetIdent() << " " << (void*)this
              << ": '" << e.GetName()
              << "' received from " << e.GetSource()->GetIdent());

    if(dynamic_cast<const CDbapiClosedEvent*>(&e) != 0 ) {
        if( dynamic_cast<CStatement*>(e.GetSource()) != 0
            || dynamic_cast<CCallableStatement*>(e.GetSource()) != 0 ) {
            if( m_rs != 0 ) {
                _TRACE("Discarding old CDB_Result " << (void*)m_rs);
                Invalidate();
            }
        }
    }
    else if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {

        RemoveListener(e.GetSource());

        if(dynamic_cast<CStatement*>(e.GetSource()) != 0
           || dynamic_cast<CCursor*>(e.GetSource()) != 0
           || dynamic_cast<CCallableStatement*>(e.GetSource()) != 0 ) {
            _TRACE("Deleting " << GetIdent() << " " << (void*)this);
            delete this;
        }
    }
}


int CResultSet::GetColNum(const string& name) {

    unsigned int i = 0;
    for( ; i < m_rs->NofItems(); ++i ) {

        if( !NStr::Compare(m_rs->ItemName(i), name) )
            return i+1;
    }

    NCBI_DBAPI_THROW( "CResultSet::GetColNum(): invalid column name [" + name + "]" );
}

void CResultSet::CheckIdx(unsigned int idx)
{
    if( idx > m_data.size() ) {
#ifdef _DEBUG
        NcbiCerr << "CResultSet::CheckIdx(): Column index " << idx << " out of range" << endl;
        _ASSERT(0);
#else
        NCBI_DBAPI_THROW( "CResultSet::CheckIdx(): Column index" + NStr::IntToString(idx) + " out of range" );
#endif
    }
}

END_NCBI_SCOPE
/*
* $Log$
* Revision 1.39  2005/04/04 13:03:56  ssikorsk
* Revamp of DBAPI exception class CDB_Exception
*
* Revision 1.38  2004/11/16 19:59:46  kholodov
* Added: GetBlobOStream() with explicit connection
*
* Revision 1.37  2004/10/25 21:39:27  kholodov
* Fixed: moving to the next column if no data read
*
* Revision 1.36  2004/09/28 00:01:22  vakatov
* Warning fix:  CResultSet::Next() -- rightfully unhandled variants in switch()
*
* Revision 1.35  2004/07/21 19:27:27  kholodov
* Fixed: incorrect amount of rows in resulset reported
*
* Revision 1.34  2004/07/21 18:43:58  kholodov
* Added: separate row counter for resultsets
*
* Revision 1.33  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
* Revision 1.31  2004/04/26 14:14:27  kholodov
* Added: ExecteQuery() method
*
* Revision 1.30  2004/04/12 14:25:33  kholodov
* Modified: resultset caching scheme, fixed single connection handling
*
* Revision 1.29  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.28  2004/03/02 19:37:56  kholodov
* Added: process close event from CStatement to CResultSet
*
* Revision 1.27  2004/03/01 16:21:55  kholodov
* Fixed: double deletion in calling subsequently CResultset::Close() and delete
*
* Revision 1.26  2004/02/19 15:23:21  kholodov
* Fixed: attempt to delete cached CDB_Result when it was already deleted by the CResultSet object
*
* Revision 1.25  2004/02/10 18:50:44  kholodov
* Modified: made Move() method const
*
* Revision 1.24  2003/06/25 21:03:05  kholodov
* Fixed: method name in error message
*
* Revision 1.23  2003/05/05 18:32:50  kholodov
* Added: LONGCHAR and LONGBINARY support
*
* Revision 1.22  2003/02/12 15:52:32  kholodov
* Added: WasNull() method
*
* Revision 1.21  2003/02/06 19:59:22  kholodov
* Fixed: CResultSet::GetColumnNo() never returned 1
*
* Revision 1.20  2002/11/25 15:15:50  kholodov
* Removed: dynamic array module (array.hpp, array.cpp), using
* STL vector instead to keep bound column data.
*
* Revision 1.19  2002/11/07 14:50:31  kholodov
* Fixed: truncate BLOB buffer befor each successful read into CVariant object
*
* Revision 1.18  2002/10/31 22:37:05  kholodov
* Added: DisableBind(), GetColumnNo(), GetTotalColumns() methods
* Fixed: minor errors, diagnostic messages
*
* Revision 1.17  2002/10/21 20:38:08  kholodov
* Added: GetParentConn() method to get the parent connection from IStatement,
* ICallableStatement and ICursor objects.
* Fixed: Minor fixes
*
* Revision 1.16  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.15  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.14  2002/09/16 19:34:41  kholodov
* Added: bulk insert support
*
* Revision 1.13  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.12  2002/08/26 15:35:56  kholodov
* Added possibility to disable transaction log
* while updating BLOBs
*
* Revision 1.11  2002/07/08 16:08:19  kholodov
* Modified: moved initialization code to Init() method
*
* Revision 1.10  2002/07/02 13:46:05  kholodov
* Fixed: Incorrect exception class used CDB_Exception
*
* Revision 1.9  2002/07/01 13:15:11  kholodov
* Added ITDescriptor diagnostics
*
* Revision 1.8  2002/06/24 19:10:03  kholodov
* Added more trace diagnostics
*
* Revision 1.7  2002/06/11 16:22:54  kholodov
* Fixed the incorrect declaration of GetMetaData() method
*
* Revision 1.6  2002/05/16 22:09:19  kholodov
* Fixed: incorrect start of BLOB column
*
* Revision 1.5  2002/05/14 19:53:17  kholodov
* Modified: Read() returns 0 to signal end of column
*
* Revision 1.4  2002/05/13 19:07:33  kholodov
* Modified: every call to GetBlobIStream() returns new istream& object. Changed to retrieve data from several BLOB columns.
*
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
*/
