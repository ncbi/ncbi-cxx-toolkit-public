/*  $Id$
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
* File Description: Implementation of dbapi bcp
* $Log$
* Revision 1.2  2003/11/25 22:39:58  sapojnik
* new option:  -i r  use CDB_Result->ReadItem() instead of GetItem()
*
* Revision 1.1  2003/10/24 18:46:36  sapojnik
* first check-in -- may not compile yet
*
* Revision 1.2  2002/12/09 16:25:20  starchen
* remove the text files from samples
*
* Revision 1.1  2002/07/18 15:48:21  starchen
* first entry
*
* Revision 1.1  2002/07/18 15:16:44  starchen
* first entry
*
*
*============================================================================
*/
#ifndef DBAPI_BCP_DONE
#define DBAPI_BCP_DONE

#include <dbapi/driver/driver_mgr.hpp>
#include <stdio.h>

USING_NCBI_SCOPE;

const char* prnType(EDB_Type t);
const char* prnSeverity(EDB_Severity s);
bool HandleIt(const CDB_Exception* ex) ;
char* getParam(char tag, int argc, char* argv[], bool* flag= 0);
int CreateTable(CDB_Connection* con, const string& table_name);
int FetchResults(CDB_Connection* con, const string& table_name, bool readItems=false);
int DeleteTable(CDB_Connection* con, const string& table_name);

#endif
