#ifndef PENDING_OPERATION__HPP
#define PENDING_OPERATION__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <string>
#include <memory>
using namespace std;

#include <corelib/request_ctx.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"
#include "psgs_reply.hpp"
#include "ipsgs_processor.hpp"


class IPSGS_Processor;


class CPendingOperation
{
public:
    CPendingOperation(shared_ptr<CPSGS_Request>  user_request,
                      shared_ptr<CPSGS_Reply>  reply,
                      IPSGS_Processor *  processor);
    ~CPendingOperation();

    void Clear(void);
    void Start(void);
    void Peek(bool  need_wait);

    void ConnectionCancel(void)
    {
        m_ConnectionCancelled = true;
    }

public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

private:
    // Incoming request
    shared_ptr<CPSGS_Request>               m_UserRequest;
    // Outcoming reply
    shared_ptr<CPSGS_Reply>                 m_Reply;

    // The PendigOperation objects are created for each of the processor and
    // each of them is postponed. The postpone may call Start() and the
    // infrastructure calls them for all PendingOperations stored for the
    // reply. To avoid multiple Process() calls for a processor the flag below
    // is created.
    bool                                    m_Started;
    bool                                    m_ConnectionCancelled;

    unique_ptr<IPSGS_Processor>             m_Processor;
    bool                                    m_InProcess;
};

#endif

