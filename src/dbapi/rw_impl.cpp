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
						 bool log_it) : m_destroy(false), m_cdbConn(0)
{
    m_dataCmd = curCmd->SendDataCmd(item_num, datasize, log_it);
}

CxBlobWriter::CxBlobWriter(CDB_Connection* conn,
                         I_ITDescriptor &d,
                         size_t blobsize, 
                         bool log_it,
						 bool destroy) : m_destroy(destroy), m_cdbConn(conn)
{
    m_dataCmd = conn->SendDataCmd(d, blobsize, log_it);
}

ERW_Result CxBlobWriter::Write(const void* buf,
                              size_t      count,
                              size_t*     bytes_written)
{
    size_t bPut = m_dataCmd->SendChunk(buf, count);

    if( bytes_written != 0 )
        *bytes_written = bPut;

    if( bPut == 0 )
        return eRW_Eof;
    else
        return eRW_Success;
}

ERW_Result CxBlobWriter::Flush()
{
    return eRW_NotImplemented;
}

CxBlobWriter::~CxBlobWriter()
{
    try {
        delete m_dataCmd;
		if( m_destroy )
			delete m_cdbConn;
    }
    NCBI_CATCH_ALL( kEmptyStr )
}


END_NCBI_SCOPE
/*
* $Log$
* Revision 1.5  2006/02/21 14:59:23  kholodov
* Streams implemented thru Reader/Writer interface
*
* Revision 1.4  2005/12/13 17:27:04  kholodov
* Modified: renamed CBlobReader/Writer to CxBlobReader/Writer
*
* Revision 1.3  2005/11/02 15:02:25  ssikorsk
* Catch all exceptions in destructors.
*
* Revision 1.2  2005/01/31 14:21:46  kholodov
* Added: use of CDB_ITDescriptor for writing BLOBs
*
* Revision 1.1  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
*/
