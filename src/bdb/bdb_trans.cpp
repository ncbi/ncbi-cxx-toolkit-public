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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB libarary transaction class implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <bdb/bdb_trans.hpp>
#include <bdb/bdb_file.hpp>
#include <db.h>

BEGIN_NCBI_SCOPE

CBDB_Transaction::CBDB_Transaction(CBDB_Env&            env, 
                                   ETransSync           tsync,
                                   EKeepFileAssociation assoc)
 : m_Env(env),
   m_TSync(tsync),
   m_Assoc(assoc),
   m_Txn(0)
{}

CBDB_Transaction::~CBDB_Transaction()
{
    if (m_Txn) {       // Active transaction is in progress
        x_Abort(true); // Abort - ignore errors (no except. from destructor)
    }
    x_DetachFromFiles();
}

DB_TXN* CBDB_Transaction::GetTxn()
{
    if (m_Env.IsTransactional()) {
        if (m_Txn == 0) {
            m_Txn = m_Env.CreateTxn(0, 0);
        }
    }
    return m_Txn;
}

void CBDB_Transaction::Commit()
{
    if (m_Txn) {
        u_int32_t flags = 
            m_TSync == eTransSync ? DB_TXN_SYNC : DB_TXN_NOSYNC;
        int ret = m_Txn->commit(m_Txn, flags);
        m_Txn = 0;
        BDB_CHECK(ret, "DB_TXN::commit");
    }
    x_DetachFromFiles();
}

void CBDB_Transaction::Abort()
{
    x_Abort(false); // abort with full error processing
    x_DetachFromFiles();
}

void CBDB_Transaction::x_Abort(bool ignore_errors)
{
    if (m_Txn) {
        int ret = m_Txn->abort(m_Txn);
        m_Txn = 0;
        if (!ignore_errors) {
            BDB_CHECK(ret, "DB_TXN::abort");
        }
    }
}

void CBDB_Transaction::x_DetachFromFiles()
{
    if (m_Assoc == eFullAssociation) {
        NON_CONST_ITERATE(vector<CBDB_RawFile*>, it, m_TransFiles) {
            CBDB_RawFile* dbfile = *it;
            dbfile->x_RemoveTransaction(this);
        }
    }
    m_TransFiles.resize(0);
}


void CBDB_Transaction::AddFile(CBDB_RawFile* dbfile) 
{
    if (m_Assoc == eFullAssociation) {
        m_TransFiles.push_back(dbfile);
    }
}


void CBDB_Transaction::RemoveFile(CBDB_RawFile* dbfile)
{
    if (m_Assoc == eFullAssociation) {
        NON_CONST_ITERATE(vector<CBDB_RawFile*>, it, m_TransFiles) {
            if (dbfile == *it) {
                m_TransFiles.erase(it);
                break;
            }
        }
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/23 18:35:30  kuznets
 * CBDB_Transaction: added flag for non-associated transactions (perf.tuning)
 *
 * Revision 1.4  2004/09/03 13:32:52  kuznets
 * + support of async. transactions
 *
 * Revision 1.3  2004/05/17 20:55:11  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/12/16 13:44:47  kuznets
 * Added disconnect call to dependent file objects when transaction closes.
 *
 * Revision 1.1  2003/12/10 19:10:09  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
