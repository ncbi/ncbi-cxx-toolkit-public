#ifndef BDB_ENV__HPP
#define BDB_ENV__HPP

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
 * File Description: Wrapper around Berkeley DB environment
 *
 */

#include <bdb/bdb_types.hpp>

BEGIN_NCBI_SCOPE

class CBDB_Env
{
public:
    CBDB_Env();

    // Construct CBDB_Env taking ownership of external DB_ENV. 
    // Presumed that env has been opened with all required parameters
    CBDB_Env(DB_ENV* env);

    ~CBDB_Env();

    void Open(const char* db_home, int flags);
    void OpenWithLocks(const char* db_home);

    // Return underlying DB_ENV structure for low level access.
    DB_ENV* GetEnv() { return m_Env; }
private:
    CBDB_Env(const CBDB_Env&);
    CBDB_Env& operator=(const CBDB_Env&);
private:
    DB_ENV*  m_Env;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/08/27 15:03:51  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* BDB_ENV__HPP */
