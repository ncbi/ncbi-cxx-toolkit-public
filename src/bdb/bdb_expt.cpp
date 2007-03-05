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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: Berkeley DB support library. 
 *                   Exception specifications and routines.
 *
 */


#include <ncbi_pch.hpp>
#include <bdb/bdb_expt.hpp>
#include <corelib/ncbidbg.hpp>

#include <db.h>


BEGIN_NCBI_SCOPE


int CBDB_StrErrAdapt::GetErrCode(void)
{ 
    _TROUBLE;
    return 0;
};


const char* CBDB_StrErrAdapt::GetErrCodeString(int errnum)
{
    return ::db_strerror(errnum);
}


const char* CBDB_ErrnoException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eSystem:       return "eSystem";
    case eBerkeleyDB:   return "eBerkeleyDB";
    default:            return  CException::GetErrCodeString();
    }
}

bool CBDB_ErrnoException::IsNoMem() const
{
    return (BDB_GetErrno() == ENOMEM);
}

bool CBDB_ErrnoException::IsBufferSmall() const
{
    return (BDB_GetErrno() == DB_BUFFER_SMALL);
}

bool CBDB_ErrnoException::IsDeadLock() const
{
    return (BDB_GetErrno() == DB_LOCK_DEADLOCK);
}

bool CBDB_ErrnoException::IsRecovery() const
{
    return (BDB_GetErrno() == DB_RUNRECOVERY);
}


END_NCBI_SCOPE
