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
*
*
* $Log$
* Revision 1.8  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
* Revision 1.7  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.6  2004/03/12 16:27:09  sponomar
* correct nested querys
*
* Revision 1.4  2002/08/26 15:35:56  kholodov
* Added possibility to disable transaction log
* while updating BLOBs
*
* Revision 1.3  2002/07/08 16:04:15  kholodov
* Reformatted
*
* Revision 1.2  2002/05/13 19:08:44  kholodov
* Modified: source code is included in NCBI namespace
*
* Revision 1.1  2002/01/30 14:51:20  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include <ncbi_pch.hpp>
#include "blobstream.hpp"

#include <dbapi/driver/public.hpp>
#include "rs_impl.hpp"

BEGIN_NCBI_SCOPE

CBlobIStream::CBlobIStream(CResultSet* rs, streamsize bufsize)
: istream(new CByteStreamBuf(bufsize))
{
    ((CByteStreamBuf*)rdbuf())->SetRs(rs);
}

CBlobIStream::~CBlobIStream()
{
    delete rdbuf();
}

CBlobOStream::CBlobOStream(CDB_Connection* connAux,
                           I_ITDescriptor* desc,
                           size_t datasize, 
                           streamsize bufsize,
                           bool log_it)
                           : ostream(new CByteStreamBuf(bufsize)), m_desc(desc), m_conn(connAux)
{
    if( log_it ) {
        _TRACE("CBlobOStream::ctor(): Transaction log enabled");
    }
    else {
        _TRACE("CBlobOStream::ctor(): Transaction log disabled");
    }
    ((CByteStreamBuf*)rdbuf())->SetCmd(m_conn->SendDataCmd(*m_desc, datasize, log_it));
}

CBlobOStream::CBlobOStream(CDB_CursorCmd* curCmd,
                           unsigned int item_num,
                           size_t datasize, 
                           streamsize bufsize,
                           bool log_it)
                           : ostream(new CByteStreamBuf(bufsize)), m_desc(0), m_conn(0)
{
    if( log_it ) {
        _TRACE("CBlobOStream::ctor(): Transaction log enabled");
    }
    else {
        _TRACE("CBlobOStream::ctor(): Transaction log disabled");
    }
    ((CByteStreamBuf*)rdbuf())->SetCmd(curCmd->SendDataCmd(item_num, datasize, log_it));
}

CBlobOStream::~CBlobOStream()
{
    delete rdbuf();
    delete m_desc;
    delete m_conn;
}

END_NCBI_SCOPE
