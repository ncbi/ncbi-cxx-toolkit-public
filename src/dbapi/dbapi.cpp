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
* Revision 1.4  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.3  2004/04/26 14:16:56  kholodov
* Modified: recreate the command objects each time the Get...() is called
*
* Revision 1.2  2002/09/16 19:34:41  kholodov
* Added: bulk insert support
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
*
*/

#include <ncbi_pch.hpp>
#include <dbapi/dbapi.hpp>

USING_NCBI_SCOPE;

IDataSource::~IDataSource()
{
}

IConnection::~IConnection()
{
}

IStatement::~IStatement()
{
}

ICallableStatement::~ICallableStatement() 
{
}

void ICallableStatement::Execute(const string& /*sql*/)
{
}

void ICallableStatement::ExecuteUpdate(const string& /*sql*/)
{
}

IResultSet* ICallableStatement::ExecuteQuery(const string& /*sql*/)
{
    return 0;
}

IResultSet::~IResultSet()
{
}

IResultSetMetaData::~IResultSetMetaData()
{
}

ICursor::~ICursor()
{
}

IBulkInsert::~IBulkInsert()
{
}



