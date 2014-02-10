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
* File Description: stream implementation for reading and writing BLOBs
*/

#include <ncbi_pch.hpp>
#include "blobstream.hpp"

#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>
#include "rs_impl.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_BlobStream

BEGIN_NCBI_SCOPE

CBlobIStream::CBlobIStream(CResultSet* rs, streamsize bufsize)
: istream(new CByteStreamBuf(bufsize))
{
    ((CByteStreamBuf*)rdbuf())->SetRs(rs);
}

CBlobIStream::~CBlobIStream()
{
    try {
        delete rdbuf();
    }
    NCBI_CATCH_ALL_X( 1, kEmptyStr )
}

CBlobOStream::CBlobOStream(CDB_Connection* connAux,
                           I_ITDescriptor* desc,
                           size_t datasize, 
                           streamsize bufsize,
                           TBlobOStreamFlags flags,
                           bool destroyConn)
    : ostream(new CByteStreamBuf(bufsize, flags, connAux)), m_desc(desc),
      m_conn(connAux), m_destroyConn(destroyConn)
{
    _TRACE("CBlobOStream: flags = " << flags);
    ((CByteStreamBuf*)rdbuf())
        ->SetCmd(m_conn->SendDataCmd(*m_desc, datasize,
                                     (flags & fBOS_SkipLogging) == 0));
}

CBlobOStream::CBlobOStream(CDB_CursorCmd* curCmd,
                           unsigned int item_num,
                           size_t datasize, 
                           streamsize bufsize,
                           TBlobOStreamFlags flags,
                           CDB_Connection* conn)
    : ostream(new CByteStreamBuf(bufsize, flags, conn)), m_desc(NULL),
      m_conn(conn), m_destroyConn(false)
{
    _TRACE("CBlobOStream: flags = " << flags);
    ((CByteStreamBuf*)rdbuf())
        ->SetCmd(curCmd->SendDataCmd(item_num, datasize,
                                     (flags & fBOS_SkipLogging) == 0));
}

CBlobOStream::~CBlobOStream()
{
    try {
        delete rdbuf();
        delete m_desc;
        if( m_destroyConn )
            delete m_conn;
    }
    NCBI_CATCH_ALL_X( 2, kEmptyStr )
}

END_NCBI_SCOPE
