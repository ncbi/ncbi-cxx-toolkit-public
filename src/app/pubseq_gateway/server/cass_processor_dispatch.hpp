#ifndef PSGS_CASSPROCESSORDISPATCH__HPP
#define PSGS_CASSPROCESSORDISPATCH__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: cassandra processor create dispatcher
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "psgs_request.hpp"
#include "psgs_reply.hpp"
#include "ipsgs_processor.hpp"
#include "resolve_processor.hpp"
#include "annot_processor.hpp"
#include "get_processor.hpp"
#include "getblob_processor.hpp"
#include "tse_chunk_processor.hpp"
#include "accession_version_history_processor.hpp"
#include "ipg_resolve.hpp"


USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


class CPSGS_CassProcessorDispatcher : public IPSGS_Processor
{
public:
    CPSGS_CassProcessorDispatcher();
    virtual ~CPSGS_CassProcessorDispatcher();

    virtual bool CanProcess(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply) const override;
    virtual vector<string> WhatCanProcess(shared_ptr<CPSGS_Request> request,
                                          shared_ptr<CPSGS_Reply> reply) const override;
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority priority) const override;
    virtual void Process(void) override;
    virtual void Cancel(void) override;
    virtual EPSGS_Status GetStatus(void) override;
    virtual string GetName(void) const override;
    virtual string GetGroupName(void) const override;

private:
    unique_ptr<CPSGS_ResolveProcessor>                  m_ResolveProcessor;
    unique_ptr<CPSGS_GetProcessor>                      m_GetProcessor;
    unique_ptr<CPSGS_GetBlobProcessor>                  m_GetBlobProcessor;
    unique_ptr<CPSGS_AnnotProcessor>                    m_AnnotProcessor;
    unique_ptr<CPSGS_TSEChunkProcessor>                 m_TSEChunkProcessor;
    unique_ptr<CPSGS_AccessionVersionHistoryProcessor>  m_AccVerProcessor;
    unique_ptr<CPSGS_IPGResolveProcessor>               m_IPGProcessor;
};

#endif  // PSGS_CASSPROCESSORDISPATCH__HPP

