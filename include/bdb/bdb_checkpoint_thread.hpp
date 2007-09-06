#ifndef BDB__ENV_CHECKPOINT_THREAD__HPP
#define BDB__ENV_CHECKPOINT_THREAD__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Transaction checkpoint / memptrickle thread
 *                   
 *
 */

#include <util/thread_nonstop.hpp>
#include <bdb/bdb_env.hpp>

BEGIN_NCBI_SCOPE

class CBDB_Env;

/** @addtogroup BDB
 *
 * @{
 */


/// Transaction checkpoint / memptrickle thread
/// 
///
class NCBI_BDB_EXPORT CBDB_CheckPointThread : public CThreadNonStop
{
public:
    CBDB_CheckPointThread(CBDB_Env& env,
                          int      memp_trickle,
                          unsigned run_delay,
                          unsigned stop_request_poll = 10);
    ~CBDB_CheckPointThread();

    virtual void DoJob(void);

    void SetWorkFlag(CBDB_Env::TBackgroundFlags flags) { m_Flags = flags; }

    /// Set maximum number of errors this thread tolerates
    /// 0 - unlimited
    ///
    void SetMaxErrors(unsigned max_err);

private:
    CBDB_CheckPointThread(const CBDB_CheckPointThread&);
    CBDB_CheckPointThread& operator=(const CBDB_CheckPointThread&);
private:
    CBDB_Env&                   m_Env;
    CBDB_Env::TBackgroundFlags  m_Flags;
    int                         m_MempTrickle;
    unsigned                    m_ErrCnt;
    unsigned                    m_MaxErrors;
};

/* @} */


END_NCBI_SCOPE

#endif
