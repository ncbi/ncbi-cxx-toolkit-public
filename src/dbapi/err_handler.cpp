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
* File Description:  DataSource implementation
*
*
* $Log$
* Revision 1.3  2005/02/23 19:17:43  kholodov
* Added: tracing received exceptions
*
* Revision 1.2  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.1  2002/11/25 15:18:30  kholodov
* First commit
*
*
*
*/

#include <ncbi_pch.hpp>
#include "err_handler.hpp"

BEGIN_NCBI_SCOPE

CToMultiExHandler::CToMultiExHandler()
    : m_ex(0)
{
    m_ex = new CDB_MultiEx("DBAPI message collector");
}

CToMultiExHandler::~CToMultiExHandler()
{
    delete m_ex;
}

bool CToMultiExHandler::HandleIt(CDB_Exception* ex)
{
    m_ex->Push(*ex);
    _TRACE("CToMultiExHandler::HandleIt(): exception received");
       
    return true;
}

END_NCBI_SCOPE
