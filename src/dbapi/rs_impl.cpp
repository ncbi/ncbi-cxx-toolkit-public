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
#include <corelib/ncbistr.hpp>

#include "stmt_impl.hpp"
#include "cstmt_impl.hpp"
#include "conn_impl.hpp"
#include "cursor_impl.hpp"
#include "rs_impl.hpp"
#include "rsmeta_impl.hpp"
#include "dbexception.hpp"

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/error_codes.hpp>


#include <typeinfo>


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls


BEGIN_NCBI_SCOPE

CResultSet::CResultSet(CConnection* conn, CDB_Result *rs) : 
    m_conn(conn),
    m_rs(rs), 
    m_istr(0), 
    m_ostr(0), 
    m_column(-1),
    m_bindBlob(true), 
    m_disableBind(false), 
    m_wasNull(true),
    m_rd(0), 
    m_totalRows(0),
    m_LastVariantNum(0),
    m_RowReadType(eReadUnknown)
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
	_ASSERT(m_rs);

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
    try {
        Notify(CDbapiClosedEvent(this));
        FreeResources();
        Notify(CDbapiDeletedEvent(this));
        _TRACE(GetIdent() << " " << (void*)this << " deleted.");
    }
    NCBI_CATCH_ALL_X( 6, kEmptyStr )
}

const CVariant& CResultSet::GetVariant(const CDBParamVariant& param)
{
    int index = 0;

    if (param.IsPositional()) {
        index = param.GetPosition();
    } else {
        index = GetColNum(param.GetName());
    }

    CheckIdx(index);
    --index;

    if (index < m_LastVariantNum) {
        x_CacheItems(index);
    }
    else {
        switch (m_RowReadType) {
        case eReadRaw:
            // Here we will be also when IsDisableBind() == true
            m_data[index].SetNull();
            break;
        case eReadUnknown:
            m_RowReadType = eReadVariant;
            m_column = -1;
            // fall through
        //case eReadVariant:
        default:
            x_CacheItems(index);
            break;
        }
    }

    return m_data[index];
}

const IResultSetMetaData* CResultSet::GetMetaData(EOwnership ownership)
{
    CResultSetMetaData *md = new CResultSetMetaData(m_rs);
    if( ownership == eNoOwnership )
    {
        md->AddListener(this);
        AddListener(md);
    }
    return md;
}

EDB_ResType CResultSet::GetResultType()
{
	_ASSERT(m_rs);
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

	if (m_rs) {
		more = m_rs->Fetch();
        m_LastVariantNum = 0;
        m_RowReadType = eReadUnknown;
	}

    if (more  &&  m_data.size() == 0) {
        Init();
    }

    m_column = 0;
    if( more && !IsDisableBind() ) {
        for(unsigned int i = 0; i < m_rs->NofItems(); ++i ) {
            EDB_Type type = m_rs->ItemDataType(i);
            if( type == eDB_Text || type == eDB_Image )  {
                break;
            }
            ++m_column;
        }

        m_LastVariantNum = m_column;
        if ((unsigned int)m_column >= m_rs->NofItems()) {
            m_column = -1;
        }
    }
    else {
        m_RowReadType = eReadRaw;
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

void CResultSet::x_CacheItems(int last_num) {
    int ind;
    while ((ind = m_rs->CurrentItemNo()) >= 0 && ind <= last_num) {
        EDB_Type type = m_rs->ItemDataType(ind);

        CVariant& var = m_data[ind];
        if (type == eDB_Text  ||  type == eDB_Image) {
            ((CDB_Stream*)var.GetNonNullData())->Truncate();
            var.SetITDescriptor(m_rs->GetImageOrTextDescriptor());
        }
        m_rs->GetItem(var.GetNonNullData());

        if (m_rs->ResultType() == eDB_StatusResult)
            break;
    }
}

size_t CResultSet::Read(void* buf, size_t size)
{
	_ASSERT(m_rs);

    if( m_column < 0 ) {
        _TRACE("CResulstSet: No available column for Read(), current column: "
                << m_rs->CurrentItemNo());
		NCBI_DBAPI_THROW( "No available column for Read()" );
    }

    x_CacheItems(m_column - 1);
    m_RowReadType = eReadRaw;

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
	_ASSERT(m_rs);
    return m_rs->CurrentItemNo() + 1;
}

unsigned int CResultSet::GetTotalColumns()
{
	_ASSERT(m_rs);
    return m_rs->NofItems();
}

CNcbiIstream& CResultSet::GetBlobIStream(size_t buf_size)
{
    delete m_istr;
    m_istr = 0;
	m_istr = new CRStream(new CxBlobReader(this), buf_size, 0,
                          (CRWStreambuf::fOwnReader |
                           CRWStreambuf::fLogExceptions));
    return *m_istr;
}

IReader* CResultSet::GetBlobReader()
{
    delete m_rd;
    m_rd = 0;
    m_rd = new CxBlobReader(this);
    return m_rd;
}

CNcbiOstream& CResultSet::GetBlobOStream(size_t blob_size,
                                         TBlobOStreamFlags flags,
										 size_t buf_size)
{
	_ASSERT(m_conn);
	return xGetBlobOStream(m_conn->CloneCDB_Conn(), blob_size,
                           flags, buf_size, true);
}

CNcbiOstream& CResultSet::GetBlobOStream(IConnection *conn, 
										  size_t blob_size,
                                          TBlobOStreamFlags flags,
                                          size_t buf_size)
{
	_ASSERT(m_conn);
	return xGetBlobOStream(conn->GetCDB_Connection(), blob_size,
                           flags, buf_size, false);
}

CNcbiOstream& CResultSet::xGetBlobOStream(CDB_Connection *cdb_conn, 
										  size_t blob_size,
                                          TBlobOStreamFlags flags,
                                          size_t buf_size,
										  bool destroy)
{
	_ASSERT(m_rs);

    // GetConnAux() returns pointer to pooled CDB_Connection.
    // we need to delete it every time we request new one.
    // The same with ITDescriptor
    delete m_ostr;

    // Call ReadItem(0, 0) before getting text/image descriptor
    m_rs->ReadItem(0, 0);


    auto_ptr<I_ITDescriptor> desc(m_rs->GetImageOrTextDescriptor());
    if(desc.get() == NULL) {
#ifdef _DEBUG
        NcbiCerr << "CResultSet::GetBlobOStream(): zero IT Descriptor" << endl;
        _ASSERT(0);
#else
        NCBI_DBAPI_THROW( "CResultSet::GetBlobOStream(): Invalid IT Descriptor" );
#endif
    }

    m_ostr = new CWStream(new CxBlobWriter(cdb_conn, *desc, blob_size, flags, destroy),
		                   buf_size, 0, CRWStreambuf::fOwnWriter);
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


int CResultSet::GetColNum(const string& name) 
{
	_ASSERT(m_rs);

    unsigned int i = 0;
    for( ; i < m_rs->NofItems(); ++i ) {

        if( !NStr::Compare(m_rs->ItemName(i), name) )
            return i+1;
    }

    NCBI_DBAPI_THROW( "CResultSet::GetColNum(): invalid column name [" + name + "]" );
}

void CResultSet::CheckIdx(unsigned int idx)
{
    if ( idx > m_data.size() ) {
#ifdef _DEBUG
        NcbiCerr << "CResultSet::CheckIdx(): Column index " << idx << " out of range" << endl;
        _ASSERT(0);
#else
        NCBI_DBAPI_THROW( "CResultSet::CheckIdx(): Column index" + NStr::IntToString(idx) + " out of range" );
#endif
    }
}

END_NCBI_SCOPE
