#ifndef NETSCHEDULE_ROLLBACK__HPP
#define NETSCHEDULE_ROLLBACK__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetSchedule limited rollback support
 *
 */


#include "ns_clients.hpp"


BEGIN_NCBI_SCOPE

// Forward declaration
class CQueue;


class CNSRollbackInterface
{
    public:
        virtual void  Rollback(CQueue *  queue) = 0;
        virtual ~CNSRollbackInterface() {}
};



class CNSSubmitRollback : public CNSRollbackInterface
{
    public:
        CNSSubmitRollback(const CNSClientId &  client,
                          unsigned int         job_id) :
            m_Client(client), m_JobId(job_id)
        {}

        virtual ~CNSSubmitRollback() {}

    public:
        virtual void  Rollback(CQueue *  queue);

    private:
        CNSClientId     m_Client;
        unsigned int    m_JobId;
};



class CNSBatchSubmitRollback : public CNSRollbackInterface
{
    public:
        CNSBatchSubmitRollback(const CNSClientId &  client,
                               unsigned int         first_job_id,
                               size_t               batch_size) :
            m_Client(client),
            m_FirstJobId(first_job_id), m_BatchSize(batch_size)
        {}

        virtual ~CNSBatchSubmitRollback() {}

    public:
        virtual void  Rollback(CQueue *  queue);

    private:
        CNSClientId     m_Client;
        unsigned int    m_FirstJobId;
        size_t          m_BatchSize;
};



class CNSGetJobRollback : public CNSRollbackInterface
{
    public:
        CNSGetJobRollback(const CNSClientId &  client,
                          unsigned int         job_id) :
            m_Client(client), m_JobId(job_id)
        {}

        virtual ~CNSGetJobRollback() {}

    public:
        virtual void  Rollback(CQueue *  queue);

    private:
        CNSClientId     m_Client;
        unsigned int    m_JobId;
};


class CNSReadJobRollback : public CNSRollbackInterface
{
    public:
        CNSReadJobRollback(const CNSClientId &  client,
                          unsigned int         job_id) :
            m_Client(client), m_JobId(job_id)
        {}

        virtual ~CNSReadJobRollback() {}

    public:
        virtual void  Rollback(CQueue *  queue);

    private:
        CNSClientId     m_Client;
        unsigned int    m_JobId;
};



END_NCBI_SCOPE

#endif /* NETSCHEDULE_ROLLBACK__HPP */

