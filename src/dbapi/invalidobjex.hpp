#ifndef _INVALIDOBJEX_HPP_
#define _INVALIDOBJEX_HPP_

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
* File Description:  Invalid Object exception
*
*
* $Log$
* Revision 1.1  2002/01/30 14:51:23  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <corelib/ncbistd.hpp>
#include <exception>
#include <string>

USING_NCBI_SCOPE;

class CInvalidObjEx : public exception {

public:
  CInvalidObjEx(const string& errMsg) 
    : m_errMsg(errMsg) {}

  const char* what() const {
    return m_errMsg.c_str();
  }

private:
  string m_errMsg;

};


#endif // _GENERAL_EXCEPTION_HPP_
