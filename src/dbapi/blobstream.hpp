#ifndef _BLOBSTREAM_HPP_
#define _BLOBSTREAM_HPP_

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

#include "bytestreambuf.hpp"
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

class CBlobIStream : public istream
{
public:
    
    CBlobIStream(CResultSet* rs, streamsize bufsize = 0);
    
    virtual ~CBlobIStream();
};

//====================================================================
class CBlobOStream : public ostream
{
public:
    
    CBlobOStream(CDB_Connection* connAux,
	             I_ITDescriptor* desc,
                 size_t datasize, 
                 streamsize bufsize,
                 TBlobOStreamFlags flags,
                 bool destroyConn = false);
    
    CBlobOStream(CDB_CursorCmd* curCmd,
	             unsigned int item_num,
                 size_t datasize, 
                 streamsize bufsize,
                 TBlobOStreamFlags flags,
                 CDB_Connection* conn);
    
    virtual ~CBlobOStream();
    
private:
    I_ITDescriptor* m_desc;
    CDB_Connection* m_conn;
    bool m_destroyConn;
};

//====================================================================

END_NCBI_SCOPE
#endif //  _BLOBSTREAM_HPP_
