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

#include <objtools/pubseq_gateway/impl/cassandra/CassBlobOp.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/CassFactory.hpp>
USING_IDBLOB_SCOPE;

#include "HttpServerTransport.hpp"

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppPerf.hpp>
using namespace IdLogUtil;


class CPendingOperation
{
public:
    CPendingOperation(string &&  sat_name, int  sat_key,
                      shared_ptr<CCassConnection>  conn,
                      unsigned int  timeout);
    ~CPendingOperation();
    void Clear();
    void Start(HST::CHttpReply<CPendingOperation>& resp);
    void Peek(HST::CHttpReply<CPendingOperation>& resp);

    void Cancel(void)
    {
        m_Cancelled = true;
    }

public:
    CPendingOperation(const CPendingOperation&) = delete;
    CPendingOperation& operator=(const CPendingOperation&) = delete;
    CPendingOperation(CPendingOperation&&) = default;
    CPendingOperation& operator=(CPendingOperation&&) = default;

private:
    HST::CHttpReply<CPendingOperation>*     m_Reply;
    bool                                    m_Cancelled;
    bool                                    m_FinishedRead;
    const int                               m_SatKey;
    CAppOp                                  m_Op;
    unique_ptr<CCassBlobLoader>             m_Loader;
    vector<h2o_iovec_t>                     m_Chunks;
};


#endif
