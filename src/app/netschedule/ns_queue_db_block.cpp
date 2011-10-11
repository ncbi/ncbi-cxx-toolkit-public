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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: queues DB block and blocks array
 *
 */

#include <ncbi_pch.hpp>

#include "ns_types.hpp"
#include "ns_queue_db_block.hpp"


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////
// SQueueDbBlock

void SQueueDbBlock::Open(CBDB_Env& env, const string& path, int pos_)
{
    pos       = pos_;
    allocated = false;

    string      prefix = "jsq_" + NStr::IntToString(pos);
    bool        group_tables_for_queue = false;

    try {
        string      fname = prefix + ".db";
        string      tname = "";

        job_db.SetEnv(env);
        // TODO: RevSplitOff make sense only for long living queues,
        // for dynamic ones it slows down the process, but because queue
        // if eventually is disposed of, it does not make sense to save
        // space here
        job_db.RevSplitOff();

        if (group_tables_for_queue)
            tname = "job";
        job_db.Open(fname, tname, CBDB_RawFile::eReadWriteCreate);

        if (group_tables_for_queue)
            tname = "jobinfo";
        else
            fname = prefix + "_jobinfo.db";
        job_info_db.SetEnv(env);
        job_info_db.Open(fname, tname, CBDB_RawFile::eReadWriteCreate);

        if (group_tables_for_queue)
            tname = "events";
        else
            fname = prefix + "_events.db";
        events_db.SetEnv(env);
        events_db.Open(fname, tname, CBDB_RawFile::eReadWriteCreate);

        if (group_tables_for_queue)
            tname = "deleted";
        else
            fname = prefix + "_deleted.db";
        deleted_jobs_db.SetEnv(env);
        deleted_jobs_db.Open(fname, tname, CBDB_RawFile::eReadWriteCreate);

        if (group_tables_for_queue)
            tname = "affdict";
        else
            fname = prefix + "_affdict.db";
        aff_dict_db.SetEnv(env);
        aff_dict_db.Open(fname, tname, CBDB_RawFile::eReadWriteCreate);

        if (group_tables_for_queue)
            tname = "start_from";
        else
            fname = prefix + "_start_from.idx";
        start_from_db.SetEnv(env);
        start_from_db.Open(fname, tname, CBDB_RawFile::eReadWriteCreate);

    } catch (CBDB_ErrnoException&) {
        throw;
    }
}


void SQueueDbBlock::x_InitStartCounter(void)
{
    // Checks if there are any records in the start_from_db
    // If not then creates a record with an initial value
    start_from_db.pseudo_key = 1;
    if (start_from_db.Fetch() == eBDB_NotFound) {
        // There are no records, create the initial one
        start_from_db.pseudo_key = 1;
        start_from_db.start_from = 0;
        start_from_db.UpdateInsert();
    }
}


void SQueueDbBlock::Close()
{
    start_from_db.Close();
    aff_dict_db.Close();
    deleted_jobs_db.Close();
    events_db.Close();
    job_info_db.Close();
    job_db.Close();
}


void SQueueDbBlock::Truncate()
{
    start_from_db.SafeTruncate();
    aff_dict_db.SafeTruncate();
    deleted_jobs_db.SafeTruncate();
    events_db.SafeTruncate();
    job_info_db.SafeTruncate();
    job_db.SafeTruncate();

    CBDB_Env& env = *job_db.GetEnv();
    env.ForceTransactionCheckpoint();
    env.CleanLog();
}



//////////////////////////////////////////////////////////////////////////
// CQueueDbBlockArray

CQueueDbBlockArray::CQueueDbBlockArray()
  : m_Count(0), m_Array(0)
{}


CQueueDbBlockArray::~CQueueDbBlockArray()
{}


void CQueueDbBlockArray::Init(CBDB_Env& env, const string& path,
                             unsigned count)
{
    m_Count = count;
    m_Array = new SQueueDbBlock[m_Count];

    for (unsigned n = 0; n < m_Count; ++n) {
        m_Array[n].Open(env, path, n);
    }
}


void CQueueDbBlockArray::Close()
{
    for (unsigned n = 0; n < m_Count; ++n) {
        m_Array[n].Close();
    }

    delete [] m_Array;
    m_Array = 0;
    m_Count = 0;
}


int CQueueDbBlockArray::Allocate()
{
    for (unsigned n = 0; n < m_Count; ++n) {
        if (!m_Array[n].allocated) {
            m_Array[n].allocated = true;
            return n;
        }
    }
    return -1;
}


bool CQueueDbBlockArray::Allocate(int pos)
{
    if (pos < 0  || pos >= int(m_Count))
        return false;

    if (m_Array[pos].allocated)
        return false;

    m_Array[pos].allocated = true;
    return true;
}


SQueueDbBlock* CQueueDbBlockArray::Get(int pos)
{
    if (pos < 0 || unsigned(pos) >= m_Count)
        return NULL;

    return &m_Array[pos];
}


END_NCBI_SCOPE

