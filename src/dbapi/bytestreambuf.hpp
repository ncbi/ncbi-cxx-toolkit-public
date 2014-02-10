#ifndef _BYTESTREAMBUF_HPP_
#define _BYTESTREAMBUF_HPP_

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
* File Description:  streambuf implementation for BLOBs
*/

#include <dbapi/dbapi.hpp>

BEGIN_NCBI_SCOPE

class CDB_SendDataCmd;
class CResultSet;

class CByteStreamBuf : public streambuf
{
public:
    CByteStreamBuf(streamsize bufsize, TBlobOStreamFlags flags = 0,
                   CDB_Connection* conn = NULL);
    virtual ~CByteStreamBuf();

    void SetCmd(CDB_SendDataCmd* cmd);
    void SetRs(CResultSet* rs);

protected:
    virtual streambuf* setbuf(CT_CHAR_TYPE* p, streamsize n);
    virtual CT_INT_TYPE underflow();
    virtual streamsize showmanyc();

    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual int sync();
  

private:

    CT_CHAR_TYPE* getGBuf();
    CT_CHAR_TYPE* getPBuf();

    CT_CHAR_TYPE* m_buf;
    size_t m_size;
    //streamsize m_len;
    CResultSet* m_rs;
    CDB_SendDataCmd* m_cmd;
    //int m_column;
    auto_ptr<CAutoTrans> m_AutoTrans;
};

END_NCBI_SCOPE
//=================================================================
#endif // _BYTESTREAMBUF_HPP_
