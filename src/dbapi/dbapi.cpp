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
*/

#include <ncbi_pch.hpp>
#include <dbapi/dbapi.hpp>

USING_NCBI_SCOPE;

IDataSource::~IDataSource()
{
}

void IDataSource::SetApplicationName(const string& app_name)
{
	GetDriverContext()->SetApplicationName(app_name);
}

string IDataSource::GetApplicationName(void) const
{
	return GetDriverContext()->GetApplicationName();
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

void ICallableStatement::SendSql(const string& /*sql*/)
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
