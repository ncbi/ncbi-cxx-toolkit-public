#ifndef _RW_IMPL_HPP_
#define _RW_IMPL_HPP_

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

#include <dbapi/dbapi.hpp>

BEGIN_NCBI_SCOPE

class CDB_SendDataCmd;
class CResultSet;

class CBlobReader : public IReader
{
public:
    CBlobReader(class CResultSet *rs);

    virtual ~CBlobReader();
  
    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

private:
    
    CResultSet *m_rs;
};

class CBlobWriter : public IWriter
{
public:

    CBlobWriter(CDB_CursorCmd* curCmd,
                unsigned int item_num,
                size_t datasize, 
                bool log_it);

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

    virtual ~CBlobWriter();

private:

    CDB_SendDataCmd *dataCmd;
};

END_NCBI_SCOPE

#endif // _RW_IMPL_HPP_

/*
* $Log$
* Revision 1.2  2004/07/20 20:23:33  ucko
* Place forward declarations outside classes to avoid confusing WorkShop.
*
* Revision 1.1  2004/07/20 17:49:17  kholodov
* Added: IReader/IWriter support for BLOB I/O
*
*/
