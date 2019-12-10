#ifndef ASYNC_BIOSEQ_QUERY__HPP
#define ASYNC_BIOSEQ_QUERY__HPP

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
 * File Description:
 *
 */


#include <corelib/request_status.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>


class CPendingOperation;



class CAsyncBioseqQuery
{
public:
    CAsyncBioseqQuery(SBioseqResolution &&  bioseq_resolution,
                      CPendingOperation *   pending_op);

public:
    void MakeRequest(bool  with_seq_id_type);

public:
    void x_OnBioseqInfo(vector<CBioseqInfoRecord>&&  records);
    void x_OnBioseqInfoWithoutSeqIdType(vector<CBioseqInfoRecord>&&  records);
    void x_OnBioseqInfoError(CRequestStatus::ECode  status, int  code,
                             EDiagSev  severity, const string &  message);

private:
    SBioseqResolution                   m_BioseqResolution;
    CPendingOperation *                 m_PendingOp;
    bool                                m_NeedTrace;

    CCassFetch *                        m_Fetch;
    CCassFetch *                        m_NoSeqIdTypeFetch;
    chrono::system_clock::time_point    m_BioseqRequestStart;
};

#endif
