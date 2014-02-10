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
* File Name:  rw_impl.cpp
*
* Author:  Michael Kholodov
*   
* File Description:  Reader/writer implementation
*
*
*/
#include <ncbi_pch.hpp>
#include "rw_impl.hpp"
#include "rs_impl.hpp"
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   Dbapi_ObjImpls

BEGIN_NCBI_SCOPE

CxBlobReader::CxBlobReader(CResultSet *rs) 
: m_rs(rs)
{

}

CxBlobReader::~CxBlobReader()
{

}
  
ERW_Result CxBlobReader::Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read)
{
    size_t bRead = m_rs->Read(buf, count);

    if( bytes_read != 0 ) {
        *bytes_read = bRead;
    }

    if( bRead != 0 ) {
        return eRW_Success;
    }
    else {
        return eRW_Eof;
    }

}

ERW_Result CxBlobReader::PendingCount(size_t* /* count */)
{
    return eRW_NotImplemented;
}

//------------------------------------------------------------------------------------------------

CxBlobWriter::CxBlobWriter(CDB_CursorCmd* curCmd,
                         unsigned int item_num,
                         size_t datasize, 
                         TBlobOStreamFlags flags)
    : m_destroy(false), m_cdbConn(0), m_BytesNeeded(datasize)
{
    m_dataCmd = curCmd->SendDataCmd(item_num, datasize,
                                    (flags & fBOS_SkipLogging) == 0);
}

CxBlobWriter::CxBlobWriter(CDB_Connection* conn,
                         I_ITDescriptor &d,
                         size_t blobsize, 
                         TBlobOStreamFlags flags,
                         bool destroy)
    : m_destroy(destroy), m_cdbConn(conn), m_BytesNeeded(blobsize)
{
    if ((flags & fBOS_UseTransaction) != 0) {
        m_AutoTrans.reset(new CAutoTrans(*conn));
    }
    m_dataCmd = conn->SendDataCmd(d, blobsize,
                                  (flags & fBOS_SkipLogging) == 0);
}

ERW_Result CxBlobWriter::Write(const void* buf,
                              size_t      count,
                              size_t*     bytes_written)
{
    _ASSERT(count <= m_BytesNeeded);
    size_t bPut = m_dataCmd->SendChunk(buf, count);

    if( bytes_written != 0 )
        *bytes_written = bPut;

    m_BytesNeeded -= bPut;
    if (m_BytesNeeded == 0  &&  m_AutoTrans.get() != NULL) {
        // Promptly release the transaction.
        m_AutoTrans->Finish();
        m_AutoTrans.reset();    
    }

    if (bPut == 0) {
        m_AutoTrans.reset();
        return eRW_Eof;
    } else {
        return eRW_Success;
    }
}

ERW_Result CxBlobWriter::Flush()
{
    return eRW_NotImplemented;
}

CxBlobWriter::~CxBlobWriter()
{
    try {
        delete m_dataCmd;
    }
    NCBI_CATCH_ALL_X( 8, kEmptyStr )
    if (m_destroy) {
        delete m_cdbConn;
    }
}


END_NCBI_SCOPE
