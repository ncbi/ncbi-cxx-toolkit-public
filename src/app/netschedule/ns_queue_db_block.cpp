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

    string      prefix = "jsq_" + NStr::NumericToString(pos);

    try {
        job_db.SetEnv(env);
        // TODO: RevSplitOff make sense only for long living queues,
        // for dynamic ones it slows down the process, but because queue
        // if eventually is disposed of, it does not make sense to save
        // space here
        job_db.RevSplitOff();
        job_db.Open(prefix + ".db", "", CBDB_RawFile::eReadWriteCreate);

        job_info_db.SetEnv(env);
        job_info_db.Open(prefix + "_jobinfo.db", "",
                         CBDB_RawFile::eReadWriteCreate);

        events_db.SetEnv(env);
        events_db.Open(prefix + "_events.db", "",
                       CBDB_RawFile::eReadWriteCreate);
    } catch (CBDB_ErrnoException&) {
        throw;
    }
}


void SQueueDbBlock::Close()
{
    events_db.Close();
    job_info_db.Close();
    job_db.Close();
}


void SQueueDbBlock::Truncate()
{
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
  : m_Count(0), m_Array(NULL)
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
    m_Array = NULL;
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


bool CQueueDbBlockArray::Allocate(int  pos)
{
    if (pos < 0  || pos >= int(m_Count))
        return false;

    if (m_Array[pos].allocated)
        return false;

    m_Array[pos].allocated = true;
    return true;
}


SQueueDbBlock *  CQueueDbBlockArray::Get(int  pos)
{
    if (pos < 0 || unsigned(pos) >= m_Count)
        return NULL;

    return &m_Array[pos];
}


unsigned int  CQueueDbBlockArray::CountAvailable(void) const
{
    unsigned int    count = 0;

    for (unsigned int  n = 0; n < m_Count; ++n)
        if (m_Array[n].allocated == false)
            ++count;

    return count;
}


END_NCBI_SCOPE

