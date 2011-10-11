#ifndef NETSCHEDULE_QUEUE_DB_BLOCK__HPP
#define NETSCHEDULE_QUEUE_DB_BLOCK__HPP

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

#include <string>
#include <connect/server.hpp>
#include <corelib/ncbiobj.hpp>

#include "ns_db.hpp"


BEGIN_NCBI_SCOPE


/// Queue databases
// BerkeleyDB does not like to mix DB open/close with regular operations,
// so we open a set of db files on startup to be used as actual databases.
// This block represent a set of db files to serve one queue.
struct SQueueDbBlock
{
    void Open(CBDB_Env& env, const std::string& path, int pos);
    void Close();
    void Truncate();

    bool                    allocated; // Am I allocated?
    int                     pos;       // My own pos in array
    SJobDB                  job_db;
    SJobInfoDB              job_info_db;
    SEventsDB               events_db;
    SDeletedJobsDB          deleted_jobs_db;
    SAffinityDictDB         aff_dict_db;
    SStartCounterDB         start_from_db;

private:
    void x_InitStartCounter(void);
};


class CQueueDbBlockArray
{
public:
    CQueueDbBlockArray();
    ~CQueueDbBlockArray();
    void Init(CBDB_Env& env, const std::string& path, unsigned count);
    void Close();
    // Allocate a block from array.
    // @return position of allocated block, negative if no more free blocks
    int  Allocate();
    // Allocate a block at position
    // @return true if block is successfully allocated
    bool Allocate(int pos);
    // Return block at position 'pos' to the array
    // Can not be used due to possible deadlock, see comment in
    // CQueueDataBase::DeleteQueue
    // void Free(int pos);
    SQueueDbBlock* Get(int pos);

private:
    unsigned            m_Count;
    SQueueDbBlock *     m_Array;
};



END_NCBI_SCOPE

#endif

