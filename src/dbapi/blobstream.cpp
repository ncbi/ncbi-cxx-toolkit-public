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
* Revision 1.1  2002/01/30 14:51:20  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include "blobstream.hpp"

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/public.hpp>

CBlobIStream::CBlobIStream(CDB_Result* rs, streamsize bufsize)
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
			   streamsize bufsize)
  : ostream(new CByteStreamBuf(bufsize)), m_desc(desc), m_conn(connAux)
{
  ((CByteStreamBuf*)rdbuf())->SetCmd(m_conn->SendDataCmd(*m_desc, datasize));
}

CBlobOStream::~CBlobOStream()
{
  delete rdbuf();
  delete m_desc;
  delete m_conn;
}
